<# .SYNOPSIS Proves exact-pin consumption from a relocated clean checkout and isolated outputs. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release','ASan Debug')][string] $Configuration='Debug', [ValidateSet('x64','ARM64')][string] $Platform='x64', [string] $FixtureRoot='', [switch] $DisableStlAnnotations)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ($Platform -eq 'ARM64' -and [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -ne 'Arm64') { throw 'ARM64 consumer runtime qualification requires a native ARM64 host.' }
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
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$msbuild=Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
Import-Module (Join-Path $checkout 'Tools/ConsumerBuild.psm1') -Force
$buildIdentity=Get-DxUiConsumerBuildIdentity -DxUiRoot $checkout -MSBuildPath $msbuild -Platform $Platform -DisableStlAnnotations:$DisableStlAnnotations
$otherIdentity=Get-DxUiConsumerBuildIdentity -DxUiRoot $checkout -MSBuildPath $msbuild -Platform $Platform -DisableStlAnnotations:(-not $DisableStlAnnotations)
if ($buildIdentity.Fingerprint -eq $otherIdentity.Fingerprint) { throw 'STL annotation policy did not isolate the dependency output fingerprint.' }
& (Join-Path $checkout 'vcpkg-install.ps1') -Platform $Platform -OutputRoot $output
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
$annotationsXml = $DisableStlAnnotations.IsPresent.ToString().ToLowerInvariant()
$identityProperties = '<DxUiConsumerToolset>' + (& $escape $buildIdentity.Identity.toolset) + '</DxUiConsumerToolset>' +
    '<DxUiConsumerVCToolsVersion>' + (& $escape $buildIdentity.Identity.vcToolsVersion) + '</DxUiConsumerVCToolsVersion>' +
    '<DxUiConsumerSdkVersion>' + (& $escape $buildIdentity.Identity.windowsSdkVersion) + '</DxUiConsumerSdkVersion>' +
    '<DxUiConsumerPreferredToolArchitecture>' + (& $escape $buildIdentity.Identity.preferredToolArchitecture) + '</DxUiConsumerPreferredToolArchitecture>'
$project=@"
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
 <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="$Configuration|$Platform"><Configuration>$Configuration</Configuration><Platform>$Platform</Platform></ProjectConfiguration></ItemGroup>
 <PropertyGroup Label="Globals"><ProjectGuid>{214F0DEB-8D8A-4A9E-B9CC-21D2EA374070}</ProjectGuid><WindowsTargetPlatformVersion>10.0.26100.0</WindowsTargetPlatformVersion></PropertyGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
 <PropertyGroup Label="Configuration"><EnableASAN Condition="'`$(Configuration)'=='ASan Debug'">true</EnableASAN><CopyAsanBinariesToOutDir>true</CopyAsanBinariesToOutDir><ConfigurationType>Application</ConfigurationType><PlatformToolset>v145</PlatformToolset><UseDebugLibraries Condition="('`$(Configuration)'=='Debug' or '`$(Configuration)'=='ASan Debug')">true</UseDebugLibraries></PropertyGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
 <PropertyGroup>$identityProperties<DxUiDisableStlAnnotations>$annotationsXml</DxUiDisableStlAnnotations><DxUiRoot>$rootXml</DxUiRoot><DxUiConsumerOutputRoot>$outputXml</DxUiConsumerOutputRoot><DxUiConsumerLockFile>$lockXml</DxUiConsumerLockFile><OutDir>`$(MSBuildProjectDirectory)\bin\</OutDir><IntDir>`$(MSBuildProjectDirectory)\obj\</IntDir></PropertyGroup>
 <Import Project="$rootXml\Build\DxUi.Consumer.props" />
 <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpplatest</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><PreprocessorDefinitions>UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions><AdditionalOptions>/utf-8 /Zc:preprocessor %(AdditionalOptions)</AdditionalOptions><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><RuntimeLibrary Condition="('`$(Configuration)'=='Debug' or '`$(Configuration)'=='ASan Debug')">MultiThreadedDebugDLL</RuntimeLibrary><RuntimeLibrary Condition="'`$(Configuration)'=='Release'">MultiThreadedDLL</RuntimeLibrary></ClCompile><Link><SubSystem>Console</SubSystem></Link></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="Samples/EmbeddedControls/Main.cpp" /></ItemGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
 <Import Project="$rootXml\Build\DxUi.Consumer.targets" />
</Project>
"@
# Compile every supported helper header as the first and only include in its own translation unit.
$headerUnits = foreach ($header in @('DxUi','Typography','FocusRestore','PointerInput','AccessibilityTextUnits','NativeMenuInterop')) {
    "#include <DxUi/$header.h>" | Set-Content -LiteralPath (Join-Path $consumer "$header.cpp") -Encoding utf8
    '<ClCompile Include="' + $header + '.cpp" />'
}
# Compile the public native entrypoint without consumer diagnostic defines or private headers.
Add-Content -LiteralPath (Join-Path $consumer 'DxUi.cpp') -Encoding utf8 -Value 'IRawElementProviderFragmentRoot* AcquirePublicNativeProvider(HWND hwnd) noexcept { return DxUi::CreateWindowHostAccessibilityProvider(hwnd); }'
$project = $project.Replace('<ClCompile Include="Samples/EmbeddedControls/Main.cpp" />', '<ClCompile Include="Samples/EmbeddedControls/Main.cpp" />' + ($headerUnits -join ''))
$projectPath=Join-Path $consumer 'ExternalConsumer.vcxproj'
$project | Set-Content -LiteralPath $projectPath -Encoding utf8
& $msbuild $projectPath /nologo /m /verbosity:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform"
if ($LASTEXITCODE -ne 0) { throw 'The external public consumer failed to build.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --output (Join-Path $consumer 'public-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated public consumer failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --complex-ui --output (Join-Path $consumer 'complex-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated independent complex sample failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --text-input --output (Join-Path $consumer 'text-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated text-service consumer failed.' }
if (-not (Test-Path (Join-Path $output "$Platform/$Configuration/DxUi.lib")) -or (Test-Path (Join-Path $checkout ".build/$Platform/$Configuration/DxUi.lib"))) { throw 'Consumer outputs are not isolated.' }
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
foreach ($mismatch in @('DxUiConsumerToolset=invalid','DxUiConsumerVCToolsVersion=0.0','DxUiConsumerSdkVersion=0.0','DxUiConsumerPreferredToolArchitecture=invalid','DxUiDisableStlAnnotations=invalid')) {
    & $msbuild $projectPath /nologo /verbosity:quiet /t:ValidateDxUiConsumerPin "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:$mismatch" *> (Join-Path $consumer ($mismatch.Split('=')[0]+'.log'))
    if ($LASTEXITCODE -eq 0) { throw "Consumer accepted incompatible build identity: $mismatch" }
}
$reports=Join-Path $PSScriptRoot '.build/reports';New-Item -ItemType Directory -Path $reports -Force | Out-Null
@{suite='ExternalConsumer';commit=$revision;configuration=$Configuration;platform=$Platform;
    nativeArchitecture=[System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString();
    disableStlAnnotations=$DisableStlAnnotations.IsPresent;buildIdentity=$buildIdentity;exitCode=0;fixture=$root;negativeChecks=10;
    executableSha256=(Get-FileHash (Join-Path $consumer 'bin/ExternalConsumer.exe')).Hash;
    librarySha256=(Get-FileHash (Join-Path $output "$Platform/$Configuration/DxUi.lib")).Hash;
    completedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $reports "ExternalConsumer-$Platform-$Configuration-annotationsDisabled$annotationsXml.json") -Encoding utf8
Write-Host "PASS relocated exact-pin consumer, rendering and 10 rejected pin/build mismatches: $root"
# The final native invocation deliberately failed; report this test's actual outcome
# to callers (including the GitHub Actions pwsh wrapper).
$global:LASTEXITCODE = 0
