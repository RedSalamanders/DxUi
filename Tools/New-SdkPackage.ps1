<#
.SYNOPSIS
Packages the exact Release archive as a relocatable DxUi SDK ZIP and runs an extracted SDK-only consumer.
.DESCRIPTION
Copies only distributable SDK inputs, then builds/runs the consumer outside any
producer checkout. The compiler's header dependency report must contain only the
extracted SDK and selected Visual Studio/Windows SDK. Keeps failed evidence.
.PARAMETER Platform
Matching native x64 or ARM64 host and library.
.PARAMETER Version
Frozen batch version (0.1.build).
.PARAMETER SmokeRoot
Existing approved workspace outside a Git checkout for SDK consumer scratch.
Hosted runs default to RUNNER_TEMP; local invocation requires an explicit root.
.OUTPUTS
.build/sdk/DxUi-SDK-<version>-<platform>.zip and source/compiler/archive identity receipt.
.NOTES
Requires a completed production-only Release build, PowerShell 7.4 and the selected MSVC 14.52 toolchain.
Copies public headers, restored dependencies, licenses, Release library and matching compiler PDB, integration props
and the unchanged reviewed native build policy. SDK headers are external dependencies of
the extracted consumer; its own translation units retain /Wall and zero-warning validation.
The smoke compiles its own consumer against extracted files only; it never rebuilds DxUi.
Normal-verbosity build logs retain the completed diagnostics summaries required by the zero-warning gate.
The producer PDB is opened exclusively during the extracted link and a separate missing-PDB negative control;
an active user of the producer PDB makes qualification fail rather than weakening isolation.
.EXAMPLE
./Tools/New-SdkPackage.ps1 -Platform x64 -Version 0.1.1234 -SmokeRoot Z:/RedSalamander.Perf
#>
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Platform,
    [Parameter(Mandatory)][ValidatePattern('^0\.1\.[0-9]+$')][string]$Version,
    [string]$SmokeRoot='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SmokeRoot -and $env:GITHUB_ACTIONS -eq 'true' -and $env:RUNNER_ENVIRONMENT -eq 'github-hosted'){$SmokeRoot=$env:RUNNER_TEMP}
if(-not $SmokeRoot){throw 'Pass an existing approved SmokeRoot outside a Git checkout for local SDK qualification.'}
$SmokeRoot=[IO.Path]::GetFullPath($SmokeRoot).TrimEnd('\','/')
if(-not(Test-Path -LiteralPath $SmokeRoot -PathType Container) -or
    $SmokeRoot -eq [IO.Path]::GetPathRoot($SmokeRoot).TrimEnd('\','/')){throw 'SDK SmokeRoot must be an existing workspace, never a drive root.'}
for($component=$SmokeRoot;$component;$component=[IO.Path]::GetDirectoryName($component)){
    if((Get-Item -LiteralPath $component -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'SDK SmokeRoot cannot traverse reparse points.'}
}
$null=& git -C $SmokeRoot rev-parse --show-toplevel 2>$null
if($LASTEXITCODE -eq 0){throw 'SDK SmokeRoot must be outside all producer Git checkouts.'}
Import-Module (Join-Path $root 'Tools/VisualStudio.psm1') -Force
Import-Module (Join-Path $root 'Tools/ConsumerBuild.psm1') -Force
Import-Module (Join-Path $root 'Build/BuildConsole.psm1') -Force
$installation=Get-DxUiVisualStudioInstallation
$hostArchitecture=if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq 'Arm64'){'arm64'}else{'amd64'}
$msbuild=Join-Path $installation "MSBuild/Current/Bin/$hostArchitecture/MSBuild.exe"
if(-not(Test-Path -LiteralPath $msbuild)){throw 'Native MSBuild is missing from the selected installation.'}
$identity=Get-DxUiConsumerBuildIdentity -DxUiRoot $root -MSBuildPath $msbuild -Platform $Platform
$library=Join-Path $root ".build/$Platform/Release/DxUi.lib"
$libraryPdb=Join-Path $root ".build/$Platform/Release/DxUi.pdb"
if(-not(Test-Path -LiteralPath $library -PathType Leaf) -or -not(Test-Path -LiteralPath $libraryPdb -PathType Leaf)){
    throw 'Build the production Release library and its compiler PDB before packaging.'
}
$libraryHash=(Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash.ToLowerInvariant()
$libraryPdbHash=(Get-FileHash -LiteralPath $libraryPdb -Algorithm SHA256).Hash.ToLowerInvariant()
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
Copy-Item -LiteralPath $libraryPdb -Destination (Join-Path $stage "lib/$Platform/Release/DxUi.pdb")
if((Get-FileHash -LiteralPath (Join-Path $stage "lib/$Platform/Release/DxUi.lib") -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryHash -or
   (Get-FileHash -LiteralPath (Join-Path $stage "lib/$Platform/Release/DxUi.pdb") -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryPdbHash){
    throw 'The staged library or compiler PDB changed during packaging.'
}
New-Item -ItemType Directory -Path (Join-Path $stage 'build') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Build/NativeBuildPolicy.targets') -Destination (Join-Path $stage 'build/NativeBuildPolicy.targets')
New-Item -ItemType Directory -Path (Join-Path $stage 'licenses') | Out-Null
foreach($name in @('LICENSE.txt','THIRD-PARTY-NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $stage 'licenses')}
Copy-Item -LiteralPath (Join-Path $installed 'share/wil/copyright') -Destination (Join-Path $stage 'licenses/WIL-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'docs/licenses/katex.txt') -Destination (Join-Path $stage 'licenses/katex-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'docs/licenses/tomlplusplus.txt') -Destination (Join-Path $stage 'licenses/tomlplusplus-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'docs/licenses/rapidyaml.txt') -Destination (Join-Path $stage 'licenses/rapidyaml-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'docs/licenses/rapidyaml-bundled.txt') -Destination (Join-Path $stage 'licenses/rapidyaml-bundled-NOTICES.txt')
Copy-Item -LiteralPath (Join-Path $root 'ThirdParty/libfyaml/0.9.4/LICENSE') -Destination (Join-Path $stage 'licenses/libfyaml-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'ThirdParty/libfyaml/0.9.4/LICENSE-xxHash.txt') -Destination (Join-Path $stage 'licenses/libfyaml-xxHash-LICENSE.txt')
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
Keep DxUi.pdb beside DxUi.lib so the linker can include the library's debug information.
This archive contains no producer source, producer test binaries, SDK-private test headers or Debug/ASan archive.
Public source-pin consumers retain the separate Build/DxUi.Consumer.props contract in the source repository.
The included native policy preserves /Wall for your sources and the producer's reviewed diagnostic exceptions.
'@ | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding utf8
$manifest=[ordered]@{schemaVersion=1;version=$Version;platform=$Platform;configuration='Release';kind='sdk';
    source=(& git -C $root rev-parse HEAD).Trim();apiRevision=$identity.Identity.apiRevision;compiler=$identity.Identity;
    librarySha256=$libraryHash;libraryPdbSha256=$libraryPdbHash;runtime='MD';testsIncluded=$false}
$manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $stage 'sdk-manifest.json') -Encoding utf8
New-Item -ItemType Directory -Path (Join-Path $stage 'samples/Smoke') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'Samples/SdkSmoke/Main.cpp') -Destination (Join-Path $stage 'samples/Smoke/Main.cpp')
New-Item -ItemType Directory -Path (Join-Path $stage 'samples/Shared') -Force | Out-Null
foreach($name in @('FailureHost.h','FailureHost.cpp')){Copy-Item -LiteralPath (Join-Path $root "Samples/Shared/$name") -Destination (Join-Path $stage 'samples/Shared')}
$archive=Join-Path $directory "DxUi-SDK-$Version-$Platform.zip"
if(Test-Path -LiteralPath $archive){throw 'A previous SDK ZIP already occupies this identity; use a fresh production workspace.'}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
$smoke=Join-Path $SmokeRoot ('dxui-sdk-'+[guid]::NewGuid().ToString('N'))
if(Test-Path -LiteralPath $smoke){throw 'SDK consumer scratch identity is already occupied.'}
$extracted=Join-Path $smoke 'extracted SDK'
Expand-Archive -LiteralPath $archive -DestinationPath $extracted
# Empty parent policy isolates the consumer from the source checkout's build imports.
'<Project />'|Set-Content -LiteralPath (Join-Path $smoke 'Directory.Build.props')
'<Project />'|Set-Content -LiteralPath (Join-Path $smoke 'Directory.Build.targets')
$project=@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|@PLATFORM@"><Configuration>Release</Configuration><Platform>@PLATFORM@</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{8082EFD1-95CD-4559-BBB7-98B9A4C6B203}</ProjectGuid><WindowsTargetPlatformVersion>@SDK@</WindowsTargetPlatformVersion>
    <VcpkgEnabled>false</VcpkgEnabled><VcpkgEnableManifest>false</VcpkgEnableManifest>
    <ForceImportBeforeCppTargets></ForceImportBeforeCppTargets><ForceImportAfterCppTargets></ForceImportAfterCppTargets>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries>false</UseDebugLibraries></PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup><OutDir>$(MSBuildProjectDirectory)\bin\</OutDir><IntDir>$(MSBuildProjectDirectory)\obj\</IntDir></PropertyGroup>
  <Import Project="$(MSBuildProjectDirectory)\extracted SDK\DxUi.Sdk.props" />
  <ItemDefinitionGroup><Link><SubSystem>Console</SubSystem><AdditionalDependencies>DbgHelp.lib;%(AdditionalDependencies)</AdditionalDependencies></Link><ClCompile><WarningLevel>EnableAllWarnings</WarningLevel><TreatWarningAsError>false</TreatWarningAsError><AdditionalIncludeDirectories>$(MSBuildProjectDirectory)\extracted SDK\samples\Shared;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><AdditionalOptions>/sourceDependencies "$(IntDir)%(Filename).source-dependencies.json" %(AdditionalOptions)</AdditionalOptions></ClCompile></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="extracted SDK\samples\Smoke\Main.cpp" /><ClCompile Include="extracted SDK\samples\Shared\FailureHost.cpp" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@
$project=$project.Replace('@PLATFORM@',$Platform).Replace('@SDK@',$identity.Identity.windowsSdkVersion)
$projectPath=Join-Path $smoke 'SdkSmoke.vcxproj'
[IO.File]::WriteAllText($projectPath,$project,[Text.UTF8Encoding]::new($false))
$extractedLibrary=Join-Path $extracted "lib/$Platform/Release/DxUi.lib"
$extractedPdb=Join-Path $extracted "lib/$Platform/Release/DxUi.pdb"
if((Get-FileHash -LiteralPath $extractedLibrary -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryHash -or
   (Get-FileHash -LiteralPath $extractedPdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryPdbHash){
    throw 'The extracted library or compiler PDB does not match the packaged inputs.'
}
$logs=Join-Path $root '.build/logs'
[void][IO.Directory]::CreateDirectory($logs)
$smokeLog=Join-Path $logs "sdk-consumer-$Version-$Platform.log"
# The linker must use the packaged PDB, not an absolute path in the producer.
# Deny all other access without moving or modifying the producer artifact.
$producerPdbGuard=$null
try {
    $producerPdbGuard=[IO.File]::Open($libraryPdb,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
    & $msbuild $projectPath /nologo /m /v:normal /p:Configuration=Release "/p:Platform=$Platform" "/p:VCToolsVersion=$($identity.Identity.vcToolsVersion)" 2>&1 | Tee-Object -FilePath $smokeLog | ForEach-Object {Write-Host $_}
    if($LASTEXITCODE -ne 0){throw 'Extracted SDK-only consumer failed to compile/link.'}
    Assert-NativeBuildDiagnostics -LogPath $smokeLog

    # The same consumer must expose LNK4099 when the packaged PDB is absent.
    # Only this fresh extracted copy moves, and its positive outputs stay intact.
    $heldPdb=Join-Path (Split-Path -Parent $extractedPdb) 'DxUi.pdb.missing-control'
    if(Test-Path -LiteralPath $heldPdb){throw 'SDK missing-PDB control path is already occupied.'}
    $missingLog=Join-Path $logs "sdk-missing-pdb-control-$Version-$Platform.log"
    Move-Item -LiteralPath $extractedPdb -Destination $heldPdb
    try {
        $missingArguments=@($projectPath,'/nologo','/m','/v:normal','/t:Rebuild','/p:Configuration=Release',
            ("/p:Platform=$Platform"),("/p:VCToolsVersion=$($identity.Identity.vcToolsVersion)"),
            ('/p:IntDir='+(Join-Path $smoke 'missing-pdb-obj')+'\'),('/p:OutDir='+(Join-Path $smoke 'missing-pdb-bin')+'\'))
        & $msbuild @missingArguments 2>&1 | Tee-Object -FilePath $missingLog | ForEach-Object {Write-Host $_}
        if($LASTEXITCODE -ne 0){throw 'SDK missing-PDB control must link successfully.'}
        $missingDiagnostics=Get-NativeBuildDiagnostics -LogPath $missingLog
        if($missingDiagnostics.WarningCount -lt 1 -or $missingDiagnostics.ErrorCount -ne 0 -or
            [IO.File]::ReadAllText($missingLog) -notmatch '\bLNK4099\b'){throw 'SDK missing-PDB control did not expose the missing debug database.'}
        $missingRefused=$false
        try { Assert-NativeBuildDiagnostics -LogPath $missingLog } catch { $missingRefused=$_.Exception.Message -like 'Zero-warning build validation failed:*' }
        if(-not $missingRefused){throw 'SDK missing-PDB control did not fail zero-warning validation.'}
    } finally {
        Move-Item -LiteralPath $heldPdb -Destination $extractedPdb
    }
} finally {
    if($producerPdbGuard){$producerPdbGuard.Dispose()}
}
$propertiesJson=& $msbuild $projectPath /nologo /p:Configuration=Release "/p:Platform=$Platform" "/p:VCToolsVersion=$($identity.Identity.vcToolsVersion)" -getProperty:WindowsSdkDir,VCToolsInstallDir
if($LASTEXITCODE -ne 0){throw 'Cannot evaluate the extracted consumer toolchain roots.'}
$properties=($propertiesJson -join [Environment]::NewLine|ConvertFrom-Json).Properties
$allowedRoots=@($extracted,$installation,$properties.WindowsSdkDir)|ForEach-Object{[IO.Path]::GetFullPath($_).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar}
foreach($unit in @('Main','FailureHost')){
    $dependenciesPath=Join-Path $smoke "obj/$unit.source-dependencies.json"
    $dependencies=Get-Content -LiteralPath $dependenciesPath -Raw|ConvertFrom-Json
    if(@($dependencies.Data.Includes).Count -eq 0){throw "SDK smoke $unit has no compiler header-closure proof."}
    foreach($header in $dependencies.Data.Includes){
        $full=[IO.Path]::GetFullPath($header)
        if(-not @($allowedRoots|Where-Object{$full.StartsWith($_,[StringComparison]::OrdinalIgnoreCase)}).Count){throw "SDK consumer used an unpackaged header: $header"}
    }
    Copy-Item -LiteralPath $dependenciesPath -Destination (Join-Path $logs "sdk-headers-$Version-$Platform-$unit.json")
}
& (Join-Path $smoke 'bin/SdkSmoke.exe')
if($LASTEXITCODE -ne 0){throw 'Extracted SDK-only consumer failed at runtime.'}
if((Get-FileHash -LiteralPath $extractedLibrary -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryHash -or
   (Get-FileHash -LiteralPath $extractedPdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $libraryPdbHash){
    throw 'Consumer build changed the SDK library or compiler PDB.'
}
# A real first-party warning must still be rejected after SDK headers become external.
# Keep separate outputs so this negative control cannot replace the positive consumer.
$warningProject=Join-Path $smoke 'SdkWarningControl.vcxproj'
$warningSource=Join-Path $smoke 'WarningControl.cpp'
[IO.File]::WriteAllText($warningSource,'int main() { int unused = 1; return 0; }')
$warningXml=$project.Replace('Include="extracted SDK\samples\Smoke\Main.cpp"','Include="WarningControl.cpp"').Replace('<ClCompile Include="extracted SDK\samples\Shared\FailureHost.cpp" />','')
[IO.File]::WriteAllText($warningProject,$warningXml)
$warningLog=Join-Path $logs "sdk-warning-control-$Version-$Platform.log"
$warningArguments=@($warningProject,'/nologo','/m','/v:normal','/p:Configuration=Release',
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
# A concurrent producer replacement cannot be certified as the staged library/PDB pair.
foreach($entry in @(@($library,$libraryHash),@($libraryPdb,$libraryPdbHash),@($extractedLibrary,$libraryHash),@($extractedPdb,$libraryPdbHash))){
    if((Get-FileHash -LiteralPath $entry[0] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry[1]){
        throw "SDK library or compiler PDB changed during qualification: $($entry[0])"
    }
}
$manifest['smokePassed']=$true
$manifest['producerPdbIsolationPassed']=$true
$manifest['missingPackagedPdbControlPassed']=$true
$manifest['firstPartyWarningControlPassed']=$true
$manifest['headerClosurePassed']=$true
$manifest['headerCount']=@($dependencies.Data.Includes).Count
$manifest['nativePolicySha256']=(Get-FileHash -LiteralPath (Join-Path $extracted 'build/NativeBuildPolicy.targets')).Hash.ToLowerInvariant()
$manifest['archiveSha256']=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
$manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($archive+'.receipt.json') -Encoding utf8
Write-Host "Qualified extracted SDK ZIP: $archive"
