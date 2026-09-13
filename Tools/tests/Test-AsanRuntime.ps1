# Exercise the actual MSBuild staging target, including a missing-runtime failure.
[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
$fixture = Join-Path $repo ('.build/ToolTests/AsanRuntime-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $fixture)
$target = [Security.SecurityElement]::Escape((Join-Path $repo 'Build/DxUi.AddressSanitizer.targets'))
$project = @"
<Project DefaultTargets="Stage" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="$target" />
  <Target Name="AddAsanBinariesToCopy" />
  <Target Name="Stage" DependsOnTargets="AddAsanBinariesToCopy">
    <Copy SourceFiles="@(CopyFileToFolders)" DestinationFolder="`$(MSBuildProjectDirectory)\staged" />
  </Target>
</Project>
"@
$projectPath = Join-Path $fixture 'Runtime.proj'
$project | Set-Content -LiteralPath $projectPath -Encoding utf8
$arguments = @($projectPath, '/nologo', '/verbosity:quiet', '/p:Platform=ARM64', '/p:EnableASAN=true', '/p:CopyAsanBinariesToOutDir=true', "/p:VCToolsInstallDir=$fixture\toolset\")
& $msbuild @arguments *> (Join-Path $fixture 'missing.log')
if ($LASTEXITCODE -eq 0 -or -not (Select-String -LiteralPath (Join-Path $fixture 'missing.log') -SimpleMatch 'missing its ARM64 AddressSanitizer runtime')) {
    throw 'A missing ARM64 sanitizer runtime was not rejected clearly.'
}
$runtime = Join-Path $fixture 'toolset/bin/HostArm64/arm64/clang_rt.asan_dynamic-aarch64.dll'
[void](New-Item -ItemType Directory -Path (Split-Path $runtime) -Force)
[IO.File]::WriteAllText($runtime, 'synthetic runtime staging fixture')
& $msbuild @arguments *> (Join-Path $fixture 'present.log')
if ($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $runtime).Hash -ne (Get-FileHash -LiteralPath (Join-Path $fixture 'staged/clang_rt.asan_dynamic-aarch64.dll')).Hash) {
    throw 'ARM64 sanitizer staging did not copy the selected toolset runtime.'
}
Write-Host 'PASS ARM64 AddressSanitizer runtime staging and missing-runtime rejection'
$global:LASTEXITCODE = 0
