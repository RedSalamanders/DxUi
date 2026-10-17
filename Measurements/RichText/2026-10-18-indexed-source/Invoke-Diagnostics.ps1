[CmdletBinding()] param([string] $Root,[string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path $OutputDirectory){throw 'Preserve the previous diagnostic directory; use a fresh one.'}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$started=[DateTime]::UtcNow;$results=[Collections.Generic.List[object]]::new()
$schedule=@(foreach($configuration in @('Release','Debug','ASan Debug')){foreach($timers in @('Off','On')){@{configuration=$configuration;timers=$timers}}})
@{classification='native-diagnostic-protocol-and-resource-invariants';schedule=$schedule;overallBudgetSeconds=900;nativeChildBudgetSeconds=120;
    scope='All current x64 build profiles, both timer branches and all four controls. These are separate functional diagnostics after indexed-source implementation, not a paired timing comparison with the earlier library versions.'} |
    ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt') -Encoding utf8
try {
    foreach($slot in $schedule){
        $remaining=[int][Math]::Floor((900-([DateTime]::UtcNow-$started).TotalSeconds)*1000)
        if($remaining -le 0){break}
        $stem=$slot.configuration.Replace(' ','-')+'-'+$slot.timers
        $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
        $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true;$start.WorkingDirectory=$Root
        foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'performance-profile.ps1'),'-Configuration',$slot.configuration,
            '-Platform','x64','-BoundaryTimers',$slot.timers,'-SkipBuild','-OutputPath',(Join-Path $OutputDirectory ($stem+'.json')))){$start.ArgumentList.Add($argument)}
        Write-Host "Native boundary invariants: $stem"
        $process=[Diagnostics.Process]::Start($start)
        try{
            $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
            $timedOut=-not $process.WaitForExit($remaining)
            if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
            $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
            if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.err')),$stderr.GetAwaiter().GetResult())}
            $code=if($process.HasExited){$process.ExitCode}else{$null}
            $results.Add(@{configuration=$slot.configuration;timers=$slot.timers;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained})
        }finally{$process.Dispose()}
        if($timedOut -or -not $drained -or $code -ne 0){throw "Native boundary invariant run failed: $stem"}
    }
}finally{
    @{scheduled=6;attempted=$results.Count;results=$results.ToArray();elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;acceptance='not-established'} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt') -Encoding utf8
}
if($results.Count -ne 6){throw 'Incomplete current-profile diagnostic inventory.'}
