#Requires -Version 7.2
# Preserve both public invocation forms: GetNewClosure can hide script-local helpers
# only when a fixture suite is called from another script, as the tooling runner does.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(-not $IsWindows){Write-Host 'SKIP hosted MSVC GUI entrypoints: Windows required.';return}
$suite=Join-Path $PSScriptRoot 'Test-MsvcSetup.ps1'
$packet=Join-Path $PSScriptRoot ('../../.build/msvc-setup-entrypoints/'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($packet)
$wrapper=Join-Path $packet 'Invoke-Nested.ps1'
[IO.File]::WriteAllText($wrapper,'param([string]$Suite); Set-StrictMode -Version Latest; $ErrorActionPreference="Stop"; & $Suite')
foreach($mode in @('Direct','Nested')){
    $start=[Diagnostics.ProcessStartInfo]::new('pwsh')
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $arguments=if($mode-eq 'Direct'){@('-NoProfile','-File',$suite)}else{@('-NoProfile','-File',$wrapper,'-Suite',$suite)}
    foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    try{
        [void]$process.Start();$stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit(120000)){$process.Kill($true);throw "Owned $mode setup fixture timed out."}
        $output=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $packet "$mode.log"),$output)
        Write-Host $output
        if($process.ExitCode-ne 0 -or $output-notmatch 'Hosted MSVC setup: 12 of 12 passed'){throw "$mode setup entrypoint failed; retained evidence: $packet"}
        Write-Host "PASS $mode MSVC setup entrypoint"
    }finally{$process.Dispose()}
}
$global:LASTEXITCODE=0
