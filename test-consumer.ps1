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
Import-Module (Join-Path $PSScriptRoot 'Tools/ConsumerQualification.psm1') -Force
$progress=New-ConsumerQualificationProgress -Path (Join-Path $PSScriptRoot ".build/reports/ConsumerProgress-$Platform-$Configuration-$([IO.Path]::GetFileName($root)).json") -Context @{commit=$revision;platform=$Platform;configuration=$Configuration;annotationsDisabled=[bool]$DisableStlAnnotations}
$checkout=Join-Path $root 'relocated DxUi'
$consumer=Join-Path $root 'app space'
$output=(Join-Path $consumer 'deps')+[IO.Path]::DirectorySeparatorChar
New-Item -ItemType Directory -Path $consumer -Force | Out-Null
& git clone --local --no-hardlinks $PSScriptRoot $checkout
if ($LASTEXITCODE -ne 0) { throw 'Cannot create the isolated consumer source fixture.' }
Import-Module (Join-Path $PSScriptRoot 'Tools/VisualStudio.psm1') -Force
$installation=Get-DxUiVisualStudioInstallation
$msbuild=Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
Import-Module (Join-Path $checkout 'Tools/ConsumerBuild.psm1') -Force
Import-Module (Join-Path $checkout 'Build/BuildConsole.psm1') -Force
$buildIdentity=Get-DxUiConsumerBuildIdentity -DxUiRoot $checkout -MSBuildPath $msbuild -Platform $Platform -DisableStlAnnotations:$DisableStlAnnotations
$otherIdentity=Get-DxUiConsumerBuildIdentity -DxUiRoot $checkout -MSBuildPath $msbuild -Platform $Platform -DisableStlAnnotations:(-not $DisableStlAnnotations)
if ($buildIdentity.Fingerprint -eq $otherIdentity.Fingerprint) { throw 'STL annotation policy did not isolate the dependency output fingerprint.' }
& (Join-Path $checkout 'vcpkg-install.ps1') -Platform $Platform -OutputRoot $output
$lock=Join-Path $consumer 'DxUi.lock.json'
$apiRevision=(Get-Content -LiteralPath (Join-Path $checkout 'capabilities.json') -Raw | ConvertFrom-Json).apiRevision
$pin=[ordered]@{repository='https://github.com/RedSalamanders/DxUi';commit=$revision;apiRevision=$apiRevision;targets=@('DxUi')}
function Write-Lock { $pin | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $lock -Encoding utf8 }
Write-Lock
# Prevent ancestor repository properties from contaminating this external consumer.
'<Project />' | Set-Content (Join-Path $consumer 'Directory.Build.props')
'<Project />' | Set-Content (Join-Path $consumer 'Directory.Build.targets')
foreach($name in @('EmbeddedScene.h','EmbeddedTextClient.h','GraphicsFixture.h','RichImageLoader.h','Main.cpp')) {
    $sampleDirectory=Join-Path $consumer 'Samples/EmbeddedControls'
    New-Item -ItemType Directory -Path $sampleDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $checkout "Samples/EmbeddedControls/$name") -Destination (Join-Path $sampleDirectory $name)
}
$complexDirectory=Join-Path $consumer 'Samples/ComplexUi'
New-Item -ItemType Directory -Path $complexDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $checkout 'Samples/ComplexUi/ComplexUiScene.h') -Destination $complexDirectory
Copy-Item -LiteralPath (Join-Path $checkout 'Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp') -Destination (Join-Path $consumer 'ConsumerApi.Tests.Revision4.cpp')
$sharedDirectory=Join-Path $consumer 'Samples/Shared'
New-Item -ItemType Directory -Path $sharedDirectory -Force | Out-Null
foreach($name in @('FailureHost.h','FailureHost.cpp')) { Copy-Item -LiteralPath (Join-Path $checkout "Samples/Shared/$name") -Destination $sharedDirectory }
Copy-Item -LiteralPath (Join-Path $checkout 'Tools/ConsumerApi/ConsumerApi.Tests.Revision5.cpp') -Destination (Join-Path $consumer 'ConsumerApi.Tests.Revision5.cpp')
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
 <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpplatest</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling><PreprocessorDefinitions>UNICODE;_UNICODE;WIN32_LEAN_AND_MEAN;NOMINMAX;%(PreprocessorDefinitions)</PreprocessorDefinitions><AdditionalOptions>/utf-8 /Zc:preprocessor %(AdditionalOptions)</AdditionalOptions><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><RuntimeLibrary Condition="('`$(Configuration)'=='Debug' or '`$(Configuration)'=='ASan Debug')">MultiThreadedDebugDLL</RuntimeLibrary><RuntimeLibrary Condition="'`$(Configuration)'=='Release'">MultiThreadedDLL</RuntimeLibrary></ClCompile><Link><SubSystem>Console</SubSystem><AdditionalDependencies>DbgHelp.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>
 <ItemDefinitionGroup><ClCompile><AdditionalIncludeDirectories>`$(MSBuildProjectDirectory)\Samples\ComplexUi;`$(MSBuildProjectDirectory)\Samples\Shared;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>
 <ItemGroup><ClCompile Include="Samples/EmbeddedControls/Main.cpp" /></ItemGroup>
 <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
 <Import Project="$rootXml\Build\DxUi.Consumer.targets" />
</Project>
"@
# Compile every supported helper header as the first and only include in its own translation unit.
$headerUnits = foreach ($header in @('DxUi','Failure','Typography','FocusRestore','PointerInput','AccessibilityTextUnits','NativeMenuInterop',
    'FileDrop','RichDocument','RichMarkdown','RichTextLayout','MarkdownSyntax','RichSemantic','RichSemanticDocument','RichTextCommands','RichImage','RichMath','RichTextExport')) {
    "#include <DxUi/$header.h>" | Set-Content -LiteralPath (Join-Path $consumer "$header.cpp") -Encoding utf8
    '<ClCompile Include="' + $header + '.cpp" />'
}
# Compile the public native entrypoint without consumer diagnostic defines or private headers.
Add-Content -LiteralPath (Join-Path $consumer 'DxUi.cpp') -Encoding utf8 -Value 'IRawElementProviderFragmentRoot* AcquirePublicNativeProvider(HWND hwnd) noexcept { return DxUi::CreateWindowHostAccessibilityProvider(hwnd); }'
$project = $project.Replace('<ClCompile Include="Samples/EmbeddedControls/Main.cpp" />', '<ClCompile Include="Samples/EmbeddedControls/Main.cpp" /><ClCompile Include="Samples/Shared/FailureHost.cpp" /><ClCompile Include="ConsumerApi.Tests.Revision4.cpp" /><ClCompile Include="ConsumerApi.Tests.Revision5.cpp" />' + ($headerUnits -join ''))
$projectPath=Join-Path $consumer 'ExternalConsumer.vcxproj'
$project | Set-Content -LiteralPath $projectPath -Encoding utf8
$consumerBuildLog=Join-Path $consumer 'ExternalConsumer.build.log'
& $msbuild $projectPath /nologo /m /verbosity:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/flp:logfile=$consumerBuildLog;verbosity=normal;encoding=UTF-8"
if ($LASTEXITCODE -ne 0) { throw 'The external public consumer failed to build.' }
Assert-NativeBuildDiagnostics -LogPath $consumerBuildLog
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --output (Join-Path $consumer 'public-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated public consumer failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --complex-ui --output (Join-Path $consumer 'complex-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated independent complex sample failed to render.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --text-input --output (Join-Path $consumer 'text-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated text-service consumer failed.' }
& (Join-Path $consumer 'bin/ExternalConsumer.exe') --rich-text --document-path (Join-Path $consumer 'note.md') --output (Join-Path $consumer 'rich-text-consumer.png')
if ($LASTEXITCODE -ne 0) { throw 'The relocated rich-text consumer failed to render.' }
# Independently link the same archive into an executable and two plugin-like DLLs. No DxUi C++
# state crosses their C ABI; native class dispatch and animation must remain in the owning module.
Copy-Item -LiteralPath (Join-Path $checkout 'Tests/ConsumerModules/ConsumerModules.Tests.NativeModule.cpp') -Destination $consumer
Copy-Item -LiteralPath (Join-Path $checkout 'Tests/ConsumerModules/ConsumerModules.Tests.Driver.cpp') -Destination $consumer
foreach ($moduleName in @('NativeModuleA','NativeModuleB','NativeModuleDriver')) {
    $moduleProject = [xml]$project
    $ns = [Xml.XmlNamespaceManager]::new($moduleProject.NameTable)
    $ns.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
    $moduleProject.SelectSingleNode('//m:ConfigurationType',$ns).InnerText = $(if ($moduleName -eq 'NativeModuleDriver') { 'Application' } else { 'DynamicLibrary' })
    $moduleProject.SelectSingleNode('//m:ProjectGuid',$ns).InnerText = '{' + [guid]::NewGuid().ToString().ToUpperInvariant() + '}'
    $moduleProject.SelectSingleNode('//m:IntDir',$ns).InnerText = '$(MSBuildProjectDirectory)\obj\' + $moduleName + '\'
    $compileItems = $moduleProject.SelectSingleNode('//m:ItemGroup[m:ClCompile]',$ns)
    $compileItems.RemoveAll()
    $sources = @('ConsumerModules.Tests.NativeModule.cpp')
    if ($moduleName -eq 'NativeModuleDriver') { $sources += 'ConsumerModules.Tests.Driver.cpp'; $sources += 'Samples/Shared/FailureHost.cpp' }
    foreach ($source in $sources) {
        $item = $moduleProject.CreateElement('ClCompile',$ns.LookupNamespace('m'))
        $item.SetAttribute('Include',$source)
        [void]$compileItems.AppendChild($item)
    }
    $modulePath = Join-Path $consumer "$moduleName.vcxproj"
    $moduleProject.Save($modulePath)
    $moduleBuildLog=Join-Path $consumer "$moduleName.build.log"
    & $msbuild $modulePath /nologo /m /verbosity:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/flp:logfile=$moduleBuildLog;verbosity=normal;encoding=UTF-8"
    if ($LASTEXITCODE -ne 0) { throw "The native module consumer $moduleName failed to build." }
    Assert-NativeBuildDiagnostics -LogPath $moduleBuildLog
}
$moduleOutput = @(& (Join-Path $consumer 'bin/NativeModuleDriver.exe'))
$moduleExit = $LASTEXITCODE
$moduleOutput | ForEach-Object { Write-Host $_ }
if (@($moduleOutput | Where-Object { $_ -ceq 'DXUI_NATIVE_MODULE_FATAL executed=4 passed=4' }).Count -ne 1) { throw 'Native module fatal children did not all prove their dump context and symbols.' }
$moduleResult = Set-ConsumerNativeModuleResult -Progress $progress -Lines $moduleOutput -ExitCode $moduleExit
if (-not (Test-Path (Join-Path $output "$Platform/$Configuration/DxUi.lib")) -or (Test-Path (Join-Path $checkout ".build/$Platform/$Configuration/DxUi.lib"))) { throw 'Consumer outputs are not isolated.' }
$validator=Join-Path $checkout 'Tools/validate_consumer.ps1'
function Require-Rejection([string] $scenario) {
    Invoke-ConsumerQualificationPhase -Progress $progress -Name "Pin-$scenario" -NegativeCheck -Body {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $validator -DxUiRoot $checkout -LockFile $lock *> (Join-Path $consumer "$scenario.log")
        if ($LASTEXITCODE -eq 0) { throw "Consumer validator accepted $scenario." }
    }
}
$pin.commit='0'*40;Write-Lock;Require-Rejection 'wrong-revision'
$pin.commit=$revision;$pin.apiRevision=$apiRevision-1;Write-Lock;Require-Rejection 'wrong-api'
$pin.apiRevision=$apiRevision;$pin.targets=@('Foundation');Write-Lock;Require-Rejection 'wrong-target'
$pin.targets=@('DxUi');Write-Lock
$header=Join-Path $checkout 'include/DxUi/FrameRuntime.h';$original=[IO.File]::ReadAllBytes($header)
try { Add-Content -LiteralPath $header -Value '// test fixture change';Require-Rejection 'tracked-dirty' } finally { [IO.File]::WriteAllBytes($header,$original) }
$untracked=Join-Path $checkout 'consumer-dirty.cpp'
try { Set-Content -LiteralPath $untracked -Value '// test fixture';Require-Rejection 'untracked-dirty' } finally { Remove-Item -LiteralPath $untracked }
Invoke-ConsumerQualificationPhase -Progress $progress -Name 'CleanPin' -Body {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $validator -DxUiRoot $checkout -LockFile $lock
    if ($LASTEXITCODE -ne 0) { throw 'Restored clean consumer pin was rejected.' }
}
foreach ($mismatch in @('DxUiConsumerToolset=invalid','DxUiConsumerVCToolsVersion=0.0','DxUiConsumerSdkVersion=0.0','DxUiConsumerPreferredToolArchitecture=invalid','DxUiDisableStlAnnotations=invalid')) {
    Invoke-ConsumerQualificationPhase -Progress $progress -Name $mismatch.Split('=')[0] -NegativeCheck -Body {
        & $msbuild $projectPath /nologo /verbosity:quiet /t:ValidateDxUiConsumerPin "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:$mismatch" *> (Join-Path $consumer ($mismatch.Split('=')[0]+'.log'))
        if ($LASTEXITCODE -eq 0) { throw "Consumer accepted incompatible build identity: $mismatch" }
    }
}
$reports=Join-Path $PSScriptRoot '.build/reports';New-Item -ItemType Directory -Path $reports -Force | Out-Null
Assert-ConsumerQualificationChecks -Progress $progress
@{suite='ExternalConsumer';commit=$revision;configuration=$Configuration;platform=$Platform;
    nativeArchitecture=[System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString();
    disableStlAnnotations=$DisableStlAnnotations.IsPresent;buildIdentity=$buildIdentity;exitCode=0;fixture=$root;negativeChecks=$progress.Record.negativeChecks;
    nativeModuleExecuted=$moduleResult.Executed;nativeModuleChecks=$moduleResult.Passed;executableSha256=(Get-FileHash (Join-Path $consumer 'bin/ExternalConsumer.exe')).Hash;
    nativeModuleFatalChecks=4;
    richTextCaptureSha256=(Get-FileHash (Join-Path $consumer 'rich-text-consumer.png')).Hash;
    librarySha256=(Get-FileHash (Join-Path $output "$Platform/$Configuration/DxUi.lib")).Hash;
    completedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $reports "ExternalConsumer-$Platform-$Configuration-annotationsDisabled$annotationsXml.json") -Encoding utf8
Complete-ConsumerQualificationProgress -Progress $progress -FinalReceipt (Join-Path $reports "ExternalConsumer-$Platform-$Configuration-annotationsDisabled$annotationsXml.json")
Write-Host "PASS relocated exact-pin consumer, rendering and 10 rejected pin/build mismatches: $root"
# The final native invocation deliberately failed; report this test's actual outcome
# to callers (including the GitHub Actions pwsh wrapper).
$global:LASTEXITCODE = 0
