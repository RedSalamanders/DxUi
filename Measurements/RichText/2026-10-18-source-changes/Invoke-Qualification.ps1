[CmdletBinding()] param([string] $Root,[string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[DateTime]::UtcNow
$profiles=@(
    @{platform='x64';configuration='Release';test=$true;skipBuild=$true;
        suites='Foundation,Grid,Theme,Control,MenuExitLifetime,NewControls,TextField,MultilineText,ReadOnly,ComboBox,Tree,Tooltip,Rendering,Animation,WindowHost,InteractiveLease'},
    @{platform='x64';configuration='Debug';test=$true;skipBuild=$false},
    @{platform='x64';configuration='ASan Debug';test=$true;skipBuild=$false},
    @{platform='ARM64';configuration='Debug';test=$false;skipBuild=$false},
    @{platform='ARM64';configuration='Release';test=$false;skipBuild=$false},
    @{platform='ARM64';configuration='ASan Debug';test=$false;skipBuild=$false})
$results=[Collections.Generic.List[object]]::new()
@{classification='native-functional-and-build-qualification';profiles=$profiles;startedUtc=$started.ToString('o');
    overallBudgetSeconds=3600;ownedChildBudgetSeconds=900;notes='Serial ordinary x64 suites, no desktop takeover. Release runs the remaining16 scopes after exact current EditorControls/Embedded/Accessibility; Debug/ASan run all19. Inputs remain frozen across all native runs and six build profiles. ARM64 builds are cross-compilation only. Every test.ps1 includes the canonical benchmark; unpaired timings do not qualify regression acceptance.'} |
    ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'qualification-plan.receipt.txt') -Encoding utf8
try {
    foreach ($profile in $profiles) {
        $remaining=[Math]::Min(900,[Math]::Floor(3600-([DateTime]::UtcNow-$started).TotalSeconds))
        if ($remaining -le 0) { break }
        $stem=$profile.platform+'-'+$profile.configuration.Replace(' ','-')
        $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
        $start.UseShellExecute=$false; $start.CreateNoWindow=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
        $start.WorkingDirectory=$Root
        $script=if($profile.test){'test.ps1'}else{'build.ps1'}
        foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root $script),'-Configuration',$profile.configuration,'-Platform',$profile.platform)) { $start.ArgumentList.Add($argument) }
        if($profile.test){
            if($profile.ContainsKey('suites')){$start.ArgumentList.Add('-Suites');$start.ArgumentList.Add($profile.suites)}else{$start.ArgumentList.Add('-Full')}
            $start.ArgumentList.Add('-SkipTooling')
        }
        if($profile.skipBuild){$start.ArgumentList.Add('-SkipBuild')}
        Write-Host "Qualification: $stem"
        $process=[Diagnostics.Process]::Start($start)
        try {
            $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
            $timedOut=-not $process.WaitForExit([int]($remaining*1000))
            if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
            $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
            if($drained){
                [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.log')),$stdout.GetAwaiter().GetResult())
                [IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.err')),$stderr.GetAwaiter().GetResult())
            }
            $code=if($process.HasExited){$process.ExitCode}else{$null}
            $results.Add(@{platform=$profile.platform;configuration=$profile.configuration;processId=$process.Id;exitCode=$code;
                timedOut=$timedOut;drained=$drained;completedUtc=[DateTime]::UtcNow.ToString('o')})
        } finally {$process.Dispose()}
        if($timedOut -or -not $drained -or $code -ne 0){throw "Qualification failed: $stem"}
    }
} finally {
    @{scheduled=6;attempted=$results.Count;elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'qualification-completion.receipt.txt') -Encoding utf8
}
if($results.Count -ne 6){throw 'Incomplete fixed qualification inventory.'}
