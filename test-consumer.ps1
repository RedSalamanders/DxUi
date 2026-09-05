<# .SYNOPSIS Proves exact-pin consumption from a relocated clean checkout and isolated outputs. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string] $Configuration='Debug', [string] $FixtureRoot='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$revision=(& git -C $PSScriptRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot read the DxUi revision.' }
if (& git -C $PSScriptRoot status --porcelain --untracked-files=normal) { throw 'Commit the library changes before validating a release consumer pin.' }
if (-not $FixtureRoot) { $FixtureRoot=Join-Path $PSScriptRoot '.build/consumers' }
$root=Join-Path ([IO.Path]::GetFullPath($FixtureRoot)) ([guid]::NewGuid().ToString('N').Substring(0,12))
$checkout=Join-Path $root 'relocated DxUi'
$consumer=Join-Path $root 'app space'
$output=(Join-Path $consumer 'deps')+[IO.Path]::DirectorySeparatorChar
New-Item -ItemType Directory -Path $consumer -Force | Out-Null
& git clone --local --no-hardlinks $PSScriptRoot $checkout
if ($LASTEXITCODE -ne 0) { throw 'Cannot create the isolated consumer source fixture.' }
& (Join-Path $checkout 'vcpkg-install.ps1') -Platform x64 -OutputRoot $output
$lock=Join-Path $consumer 'DxUi.lock.json'
$pin=[ordered]@{repository='https://github.com/RedSalamanders/DxUi';commit=$revision;apiRevision=2;targets=@('DxUi')}
function Write-Lock { $pin | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $lock -Encoding utf8 }
Write-Lock
# Prevent ancestor repository properties from contaminating this external consumer.
'<Project />' | Set-Content (Join-Path $consumer 'Directory.Build.props')
'<Project />' | Set-Content (Join-Path $consumer 'Directory.Build.targets')
foreach($name in @('EmbeddedScene.h','EmbeddedTextClient.h','GraphicsFixture.h','Main.cpp')) {
    $sampleDirectory=Join-Path $consumer 'Samples/EmbeddedControls'
    New-Item -ItemType Directory -Path $sampleDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $checkout "Samples/EmbeddedControls/$name") -Destination (Join-Path $sampleDirectory $name)
}
$complexDirectory=Join-Path $consumer 'Samples/ComplexUi'
New-Item -ItemType Directory -Path $complexDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $checkout 'Samples/ComplexUi/ComplexUiScene.h') -Destination $complexDirectory
$escape={param($value) [System.Security.SecurityElement]::Escape($value)}
$rootXml=& $escape $checkout
$outputXml=& $escape $output
$lockXml=& $escape $lock
$project=@"
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
 <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="$Configuration|x64"><Configuration>$Configuration</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
 <PropertyGroup Label="Globals"><ProjectGuid>{214F0DEB-8D8A-4A9E-B9CC-21D2EA374070}</ProjectGuid><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
 <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries Condition="'`$(Configuration)'=='Debug'">true</UseDebugLibraries></PropertyGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
 <PropertyGroup><DxUiRoot>$rootXml</DxUiRoot><DxUiConsumerOutputRoot>$outputXml</DxUiConsumerOutputRoot><DxUiConsumerLockFile>$lockXml</DxUiConsumerLockFile><OutDir>`$(MSBuildProjectDirectory)\bin\</OutDir><IntDir>`$(MSBuildProjectDirectory)\obj\</IntDir></PropertyGroup>
 <Import Project="$rootXml\Build\DxUi.Consumer.props" />
 <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpplatest</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><PreprocessorDefinitions>UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions><AdditionalOptions>/utf-8 /Zc:preprocessor %(AdditionalOptions)</AdditionalOptions><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><RuntimeLibrary Condition="'`$(Configuration)'=='Debug'">MultiThreadedDebugDLL</RuntimeLibrary><RuntimeLibrary Condition="'`$(Configuration)'=='Release'">MultiThreadedDLL</RuntimeLibrary></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="Samples/EmbeddedControls/Main.cpp" /></ItemGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
 <Import Project="$rootXml\Build\DxUi.Consumer.targets" />
</Project>
"@
$projectPath=Join-Path $consumer 'ExternalConsumer.vcxproj'
$project | Set-Content -LiteralPath $projectPath -Encoding utf8
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$msbuild=Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild $projectPath /nologo /m /verbosity:minimal "/p:Configuration=$Configuration" /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw 'The external public consumer failed to build.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --output (Join-Path $consumer 'public-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated public consumer failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --complex-ui --output (Join-Path $consumer 'complex-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated independent complex sample failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --text-input --output (Join-Path $consumer 'text-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated text-service consumer failed.' }
if (-not (Test-Path (Join-Path $output "x64/$Configuration/DxUi.lib")) -or (Test-Path (Join-Path $checkout ".build/x64/$Configuration/DxUi.lib"))) { throw 'Consumer outputs are not isolated.' }
$validator=Join-Path $checkout 'Tools/validate_consumer.ps1'
function Require-Rejection([string] $scenario) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $validator -DxUiRoot $checkout -LockFile $lock *> (Join-Path $consumer "$scenario.log")
    if ($LASTEXITCODE -eq 0) { throw "Consumer validator accepted $scenario." }
}
$pin.commit='0'*40;Write-Lock;Require-Rejection 'wrong-revision'
$pin.commit=$revision;$pin.apiRevision=1;Write-Lock;Require-Rejection 'wrong-api'
$pin.apiRevision=2;$pin.targets=@('Foundation');Write-Lock;Require-Rejection 'wrong-target'
$pin.targets=@('DxUi');Write-Lock
$header=Join-Path $checkout 'include/DxUi/FrameRuntime.h';$original=[IO.File]::ReadAllBytes($header)
try { Add-Content -LiteralPath $header -Value '// test fixture change';Require-Rejection 'tracked-dirty' } finally { [IO.File]::WriteAllBytes($header,$original) }
$untracked=Join-Path $checkout 'consumer-dirty.cpp'
try { Set-Content -LiteralPath $untracked -Value '// test fixture';Require-Rejection 'untracked-dirty' } finally { Remove-Item -LiteralPath $untracked }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $validator -DxUiRoot $checkout -LockFile $lock
if ($LASTEXITCODE -ne 0) { throw 'Restored clean consumer pin was rejected.' }
$reports=Join-Path $PSScriptRoot '.build/reports';New-Item -ItemType Directory -Path $reports -Force | Out-Null
@{suite='ExternalConsumer';commit=$revision;configuration=$Configuration;exitCode=0;fixture=$root;negativeChecks=5;completedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $reports "ExternalConsumer-x64-$Configuration.json") -Encoding utf8
Write-Host "PASS relocated exact-pin consumer, rendering and 5 rejected invalid pins: $root"
