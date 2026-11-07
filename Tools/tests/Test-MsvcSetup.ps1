#Requires -Version 7.2
# The delayed GUI fixture replaces download/signature/discovery boundaries; no VS installer runs.
[CmdletBinding()]param([string]$SetupScriptPath='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(-not $SetupScriptPath){$SetupScriptPath=Join-Path $repository '.github/actions/setup-msvc/Setup.ps1'}
if(-not $IsWindows){Write-Host 'SKIP hosted MSVC GUI bootstrapper fixtures: Windows process semantics required.';return}
# Retain all attempts as setup evidence.
$packet=Join-Path $repository ('.build/msvc-setup-fixtures/'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($packet)
$source=Join-Path $packet 'FakeInstaller.cs'
[IO.File]::WriteAllText($source,@'
using System;
using System.IO;
using System.Threading;
using System.Diagnostics;
class FakeInstaller {
    static int Main(string[] args) {
        string root=Environment.GetEnvironmentVariable("DXUI_SETUP_FIXTURE");
        if (args.Length > 0 && args[0] == "--child") { Thread.Sleep(30000); return 0; }
        File.WriteAllLines(Path.Combine(root,"arguments.txt"),args);
        if (Environment.GetEnvironmentVariable("DXUI_FIXTURE_CHILD") == "true") {
            Process child=Process.Start(new ProcessStartInfo(System.Reflection.Assembly.GetExecutingAssembly().Location,"--child") { CreateNoWindow=true, UseShellExecute=false });
            File.WriteAllText(Path.Combine(root,"child.txt"),child.Id+" "+child.StartTime.ToUniversalTime().Ticks);
        }
        Thread.Sleep(Int32.Parse(Environment.GetEnvironmentVariable("DXUI_FIXTURE_SLEEP")));
        string version=Environment.GetEnvironmentVariable("DXUI_FIXTURE_VERSION");
        if (!String.IsNullOrEmpty(version))
            File.WriteAllText(Path.Combine(root,"VS instance","VC","Auxiliary","Build","Microsoft.VCToolsVersion.default.txt"),version);
        File.WriteAllText(Path.Combine(root,"completed.txt"),"completed");
        File.WriteAllText(Path.Combine(Environment.GetEnvironmentVariable("TEMP"),"dd_fixture.log"),"fixture installer detail");
        Console.WriteLine("fixture bootstrapper stdout");
        Console.Error.WriteLine("fixture bootstrapper stderr");
        return Int32.Parse(Environment.GetEnvironmentVariable("DXUI_FIXTURE_EXIT"));
    }
}
'@)
$fake=Join-Path $packet 'FakeInstaller.exe'
& (Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe') /nologo /target:winexe /out:$fake $source
if($LASTEXITCODE-ne 0){throw 'Cannot compile the fake GUI bootstrapper.'}
function Invoke-SetupFixture([string]$Name,[int]$ExitCode=0,[string]$Version='14.52.37025',[switch]$Ready,[switch]$VerifyOnly,[switch]$Local,[switch]$BadSignature,[string]$Missing='', [switch]$InstallerTimeout,[switch]$DownloadTimeout,[switch]$LongTemp){
    $fixture=Join-Path $packet $Name
    $script=Join-Path $fixture '.github/actions/setup-msvc/Setup.ps1'
    [void][IO.Directory]::CreateDirectory((Split-Path $script))
    Copy-Item $SetupScriptPath $script
    if($InstallerTimeout){
        $fixtureSource=[IO.File]::ReadAllText($script).Replace('$installerTimeoutMilliseconds=1800000','$installerTimeoutMilliseconds=1500')
        [IO.File]::WriteAllText($script,$fixtureSource)
    }
    [void][IO.Directory]::CreateDirectory((Join-Path $fixture 'Tools'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Tools/VisualStudio.psm1'), 'function Get-DxUiVisualStudioInstallation { Join-Path $env:DXUI_SETUP_FIXTURE "VS instance" }; Export-ModuleMember Get-DxUiVisualStudioInstallation')
    [IO.File]::WriteAllText((Join-Path $fixture 'Directory.Build.props'),'<Project><PropertyGroup><RequiredMsvcToolsVersion>14.52</RequiredMsvcToolsVersion></PropertyGroup></Project>')
    $installation=Join-Path $fixture 'VS instance'
    $hostArch=if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()-eq 'Arm64'){'arm64'}else{'x64'}
    $files=@("VC/Tools/MSVC/14.52.37025/bin/Host$hostArch/x64/cl.exe","VC/Tools/MSVC/14.52.37025/bin/Host$hostArch/x64/link.exe",'VC/Tools/MSVC/14.52.37025/lib/x64/clang_rt.asan_dynamic-x86_64.lib','VC/Tools/Llvm/x64/bin/llvm-pdbutil.exe','VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt')
    foreach($file in $files){$path=Join-Path $installation $file;[void][IO.Directory]::CreateDirectory((Split-Path $path));if($file-ne $Missing){[IO.File]::WriteAllText($path,'')}}
    $sdk=Join-Path $fixture 'Program Files/Windows Kits/10/Include/10.0.26100.0/um/Windows.h'
    [void][IO.Directory]::CreateDirectory((Split-Path $sdk))
    if($Missing-ne 'SDK'){[IO.File]::WriteAllText($sdk,'')}
    if($Ready){[IO.File]::WriteAllText((Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'),'14.52.37025')}
    $wrapper=Join-Path $fixture 'Invoke.ps1'
    [IO.File]::WriteAllText($wrapper,@'
param([switch]$VerifyOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Invoke-WebRequest {param($Uri,$OutFile,$TimeoutSec) [IO.File]::WriteAllText((Join-Path $env:DXUI_SETUP_FIXTURE 'download-timeout.txt'),[string]$TimeoutSec); if($env:DXUI_DOWNLOAD_TIMEOUT -eq 'true'){throw [TimeoutException]::new('synthetic bounded download timeout')}; Copy-Item $env:DXUI_FAKE_INSTALLER $OutFile;[IO.File]::WriteAllText((Join-Path $env:DXUI_SETUP_FIXTURE 'downloaded.txt'),$Uri)}
function Get-AuthenticodeSignature {param($LiteralPath) [pscustomobject]@{Status=$env:DXUI_FIXTURE_SIGNATURE;SignerCertificate=[pscustomobject]@{Subject='CN=Microsoft Corporation, O=Microsoft Corporation, C=US'}}}
# Success starts with LASTEXITCODE unset, as on hosted CI. Failure has stale success.
if($env:DXUI_FIXTURE_EXIT-ne '0'){$global:LASTEXITCODE=0}
& (Join-Path $PSScriptRoot '.github/actions/setup-msvc/Setup.ps1') -Platform x64 -VerifyOnly:$VerifyOnly
'@)
    $start=[Diagnostics.ProcessStartInfo]::new('pwsh')
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($argument in @('-NoProfile','-File',$wrapper)){$start.ArgumentList.Add($argument)}
    if($VerifyOnly){$start.ArgumentList.Add('-VerifyOnly')}
    $start.Environment['DXUI_SETUP_FIXTURE']=$fixture;$start.Environment['DXUI_FAKE_INSTALLER']=$fake
    $start.Environment['DXUI_FIXTURE_VERSION']=$Version;$start.Environment['DXUI_FIXTURE_EXIT']=[string]$ExitCode
    $start.Environment['DXUI_FIXTURE_SLEEP']=if($InstallerTimeout){'30000'}else{'400'}
    $start.Environment['DXUI_FIXTURE_CHILD']=if($InstallerTimeout){'true'}else{'false'}
    $start.Environment['DXUI_DOWNLOAD_TIMEOUT']=if($DownloadTimeout){'true'}else{'false'}
    $start.Environment['DXUI_FIXTURE_SIGNATURE']=if($BadSignature){'NotSigned'}else{'Valid'}
    $start.Environment['ProgramFiles(x86)']=Join-Path $fixture 'Program Files'
    $start.Environment['GITHUB_ACTIONS']=if($Local){'false'}else{'true'}
    $start.Environment['RUNNER_ENVIRONMENT']='github-hosted';$start.Environment['RUNNER_TEMP']=Join-Path $fixture 'runner temp with spaces'
    if($LongTemp){$start.Environment['RUNNER_TEMP']=Join-Path $fixture ('runner-'+('p'*200))}
    $start.Environment['TEMP']=$start.Environment['RUNNER_TEMP']
    $start.Environment['GITHUB_OUTPUT']=Join-Path $fixture 'github-output.txt'
    [void][IO.Directory]::CreateDirectory($start.Environment['RUNNER_TEMP'])
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    try{
        [void]$process.Start();$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit(30000)){$process.Kill($true);throw 'Owned fake setup timed out.'}
        $message=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $fixture 'result.log'),$message)
        return [pscustomobject]@{Exit=$process.ExitCode;Text=$message;Root=$fixture}
    }finally{$process.Dispose()}
}
Invoke-TestCase 'delayed GUI success waits and preserves install path with spaces' {
    $result=Invoke-SetupFixture 'success'
    Assert-Equal 0 $result.Exit $result.Text
    Assert-True (Test-Path (Join-Path $result.Root 'completed.txt')) 'GUI completed before verification'
    $arguments=[IO.File]::ReadAllLines((Join-Path $result.Root 'arguments.txt'))
    $index=[Array]::IndexOf($arguments,'--installPath')
    Assert-Equal (Join-Path $result.Root 'runner temp with spaces/VS-Msvc-14.52') $arguments[$index+1] 'install path remains one argument'
    Assert-Contains $arguments '--wait' 'bootstrapper waits for its installer'
    Assert-Equal '180' ([IO.File]::ReadAllText((Join-Path $result.Root 'download-timeout.txt'))) 'download has a finite timeout'
}
Invoke-TestCase 'installer timeout terminates its owned descendant and preserves unrelated processes' {
    $start=[Diagnostics.ProcessStartInfo]::new('pwsh');$start.UseShellExecute=$false;$start.CreateNoWindow=$true
    foreach($arg in @('-NoProfile','-Command','Start-Sleep -Seconds 30')){$start.ArgumentList.Add($arg)}
    $sentinel=[Diagnostics.Process]::Start($start)
    try {
        $result=Invoke-SetupFixture 'installer-timeout' -InstallerTimeout
        Assert-True ($result.Exit-ne 0 -and $result.Text-match 'VS provisioning timed out after 1.5 seconds' -and $result.Text-match 'termination: confirmed') $result.Text
        Assert-True (-not $sentinel.HasExited) 'unrelated owned sentinel remains running'
        $identity=([IO.File]::ReadAllText((Join-Path $result.Root 'child.txt'))).Split(' ')
        $deadline=[Diagnostics.Stopwatch]::StartNew()
        $childStillOwned=$true
        do {
            $child=Get-Process -Id ([int]$identity[0]) -ErrorAction SilentlyContinue
            try {$childStillOwned=($null-ne $child -and $child.StartTime.ToUniversalTime().Ticks -eq [long]$identity[1] -and -not $child.HasExited)}
            catch [InvalidOperationException] {$childStillOwned=$false}
            if($childStillOwned){Start-Sleep -Milliseconds 50}
        } while($childStillOwned -and $deadline.ElapsedMilliseconds -lt 5000)
        Assert-True (-not $childStillOwned) 'installer descendant no longer exists'
        $output=[IO.File]::ReadAllLines((Join-Path $result.Root 'github-output.txt'))
        $logs=($output|Where-Object {$_-like 'installer_logs=*'}).Substring('installer_logs='.Length)
        Assert-True ((Test-Path (Join-Path $logs 'bootstrapper.stdout.log')) -and (Test-Path (Join-Path $logs 'bootstrapper.stderr.log'))) 'timeout retains both bootstrapper logs'
    } finally { if(-not $sentinel.HasExited){$sentinel.Kill();[void]$sentinel.WaitForExit(5000)};$sentinel.Dispose() }
}
Invoke-TestCase 'download timeout refuses before launching the installer' {
    $result=Invoke-SetupFixture 'download-timeout' -DownloadTimeout
    Assert-True ($result.Exit-ne 0 -and $result.Text-match 'synthetic bounded download timeout') $result.Text
    Assert-Equal '180' ([IO.File]::ReadAllText((Join-Path $result.Root 'download-timeout.txt'))) 'download timeout is passed to the provider'
    Assert-True (-not(Test-Path (Join-Path $result.Root 'arguments.txt'))) 'installer never launched'
    $output=[IO.File]::ReadAllLines((Join-Path $result.Root 'github-output.txt'))
    $logs=($output|Where-Object {$_-like 'installer_logs=*'}).Substring('installer_logs='.Length)
    Assert-True ((Test-Path (Join-Path $logs 'download.log')) -and ([IO.File]::ReadAllText((Join-Path $logs 'download.log')) -match 'synthetic bounded download timeout')) 'download failure is retained for the existing log artifact'
}
Invoke-TestCase 'overlong bootstrapper path refuses before download or launch' {
    $result=Invoke-SetupFixture 'long-temp' -LongTemp
    Assert-True ($result.Exit-ne 0 -and $result.Text-match 'VS provisioning requires a shorter RUNNER_TEMP') $result.Text
    Assert-True (-not(Test-Path (Join-Path $result.Root 'downloaded.txt'))) 'unsupported native launch path never downloads'
    Assert-True (-not(Test-Path (Join-Path $result.Root 'arguments.txt'))) 'unsupported native launch path never starts a process'
    Assert-True (-not(Test-Path (Join-Path $result.Root 'github-output.txt'))) 'refusal does not publish nonexistent installer logs'
}
Invoke-TestCase 'reboot-required success is accepted only after exact tools verify' {
    $result=Invoke-SetupFixture 'reboot' -ExitCode 3010
    Assert-Equal 0 $result.Exit $result.Text
    Assert-True ($result.Text-match 'exit code: 3010') 'explicit GUI exit reported'
}
Invoke-TestCase 'GUI failure overrides stale success and retains output diagnostics' {
    $result=Invoke-SetupFixture 'failed' -ExitCode 1603
    Assert-True ($result.Exit-ne 0 -and $result.Text-match 'VS provisioning failed: 1603') $result.Text
    Assert-True ($result.Text-match 'Bootstrapper output remains') 'retained diagnostics named'
    $output=[IO.File]::ReadAllLines((Join-Path $result.Root 'github-output.txt'))
    $logs=($output|Where-Object {$_-like 'installer_logs=*'}).Substring('installer_logs='.Length)
    Assert-True (Test-Path (Join-Path $logs 'dd_fixture.log')) 'detailed installer log archived'
    Assert-True ($result.Text-match 'fixture bootstrapper stdout' -and $result.Text-match 'fixture bootstrapper stderr') 'both output tails reported'
}
Invoke-TestCase 'successful installer cannot admit another compiler family' {
    $result=Invoke-SetupFixture 'wrong-family' -Version '14.51.36231'
    Assert-True ($result.Exit-ne 0 -and $result.Text-match 'Insiders did not provide default MSVC 14.52') $result.Text
}
$missingIndex=0
foreach($missing in @("VC/Tools/MSVC/14.52.37025/bin/Host$(if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()-eq 'Arm64'){'arm64'}else{'x64'})/x64/cl.exe",'VC/Tools/MSVC/14.52.37025/lib/x64/clang_rt.asan_dynamic-x86_64.lib','VC/Tools/Llvm/x64/bin/llvm-pdbutil.exe','SDK')){
    $missingCase='missing-'+(++$missingIndex)
    Invoke-TestCase "successful installer cannot admit missing $missing" {
        # The run already owns a unique packet. Compact, distinct child ids
        # keep the fake GUI executable inside the Windows launch path bound.
        $result=Invoke-SetupFixture $missingCase -Missing $missing
        Assert-True ($result.Exit-ne 0 -and $result.Text-match 'Insiders did not provide') $result.Text
    }
}
Invoke-TestCase 'verified existing tools never download or install' {
    $result=Invoke-SetupFixture 'ready' -Ready -VerifyOnly
    Assert-Equal 0 $result.Exit $result.Text
    Assert-True (-not(Test-Path (Join-Path $result.Root 'downloaded.txt'))) 'no download'
}
foreach($mode in @('VerifyOnly','Local','BadSignature')){
    Invoke-TestCase "$mode refuses before installer launch" {
        $options=@{};$options[$mode]=$true
        $result=Invoke-SetupFixture $mode @options
        Assert-True ($result.Exit-ne 0) $result.Text
        Assert-True (-not(Test-Path (Join-Path $result.Root 'completed.txt'))) 'installer never launched'
        if($mode-ne 'BadSignature'){Assert-True (-not(Test-Path (Join-Path $result.Root 'downloaded.txt'))) 'no download'}
    }
}
Write-Host "Retained MSVC setup fixtures: $packet"
Complete-TestRun 'Hosted MSVC setup'
