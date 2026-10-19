Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root='Z:/src/DxUi-worktrees/rich-text-core-d2'
Set-Location -LiteralPath $root
$out=Join-Path $root '.build/reports/rich-semantic-uia-20261019/NativeDispatch-BeforeFix'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$object=Join-Path $root '.build/Intermediate/x64/Debug/DxUi.EmbeddedTests/Embedded.Tests.Embedded.obj'
Copy-Item -LiteralPath $object -Destination $out
Copy-Item -LiteralPath .build/reports/rich-semantic-uia-20261019/Iteration6/DxUi.lib -Destination $out
$tlogs=@(rg --files --hidden --no-ignore .build/Intermediate/x64/Debug/DxUi.EmbeddedTests -g link.command.1.tlog)
if($tlogs.Count -ne 1){throw 'Unique current native link command required'}
$arguments=((Get-Content -LiteralPath $tlogs[0] -Encoding Unicode | Where-Object {$_ -and ! $_.StartsWith('^')}) -join [Environment]::NewLine)
$arguments=[regex]::Replace($arguments,'/OUT:"[^"]+"','/OUT:"'+(Join-Path $out 'BeforeFix.exe')+'"')
$arguments=[regex]::Replace($arguments,'/PDB:"[^"]+"','/PDB:"'+(Join-Path $out 'BeforeFix.pdb')+'"')
$arguments=[regex]::Replace($arguments,'/IMPLIB:"[^"]+"','/IMPLIB:"'+(Join-Path $out 'BeforeFix.lib')+'"')
$arguments=[regex]::Replace($arguments,'/ILK:"[^"]+"','')
$arguments=$arguments.Replace('/INCREMENTAL ','/INCREMENTAL:NO ')
$arguments=$arguments.Replace($object.Replace('/','\'),(Join-Path $out 'Embedded.Tests.Embedded.obj'),[StringComparison]::OrdinalIgnoreCase)
$arguments=$arguments.Replace((Join-Path $root '.build/x64/Debug/DxUi.lib').Replace('/','\'),(Join-Path $out 'DxUi.lib'),[StringComparison]::OrdinalIgnoreCase)
Set-Content -LiteralPath (Join-Path $out 'link.rsp') -Value $arguments
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=(& $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -property installationPath).Trim()
if(!$installation){throw 'VS discovery failed'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$version=(Get-Content -LiteralPath (Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$link=Join-Path $installation ('VC/Tools/MSVC/'+$version+'/bin/Hostx64/x64/link.exe')
$batch=@('@echo off',('call "'+$vcvars+'" x64 >nul'),('"'+$link+'" @"'+(Join-Path $out 'link.rsp')+'"'), 'exit /b %errorlevel%')
Set-Content -LiteralPath (Join-Path $out 'link.cmd') -Value $batch
& cmd.exe /d /c (Join-Path $out 'link.cmd') *> (Join-Path $out 'link.log')
if($LASTEXITCODE -ne 0){Get-Content (Join-Path $out 'link.log') -Tail 10;throw 'Negative diagnostic link failed'}
& (Join-Path $out 'BeforeFix.exe') --test-rich-semantic-uia-failures *> (Join-Path $out 'before-fix.log')
$code=$LASTEXITCODE
[ordered]@{purpose='Negative diagnostic only: source8 test object linked to retained source6 pre-fix library; no canonical build qualification';oldLibrarySha256=(Get-FileHash -LiteralPath (Join-Path $out 'DxUi.lib')).Hash;objectSha256=(Get-FileHash -LiteralPath (Join-Path $out 'Embedded.Tests.Embedded.obj')).Hash;executableSha256=(Get-FileHash -LiteralPath (Join-Path $out 'BeforeFix.exe')).Hash;exitCode=$code}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'result.json')
Get-ChildItem -LiteralPath $out -File | Get-FileHash | Select-Object Hash,Path | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'files.sha256.json')
Get-Content -LiteralPath (Join-Path $out 'before-fix.log') -Tail 12
Write-Output ('Negative diagnostic exit='+$code)
if($code -eq 0){throw 'Expected original allocation failure was not reproduced'}
