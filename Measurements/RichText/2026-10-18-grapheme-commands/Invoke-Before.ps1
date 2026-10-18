[CmdletBinding()] param([string] $Root, [string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[DateTime]::UtcNow
$slots=@(
    @{name='oracle-trials-legacy';suite='EditorControls';skipBuild=$false;tests='TestUnicode17BulkCharacterMovementOracle,TestTextElementBulkMovementFixedTrials,TestTextFieldInteriorAcpDeletion'},
    @{name='rich-native';suite='Accessibility';skipBuild=$true;tests='TestRichTextAccessibilityWindowInteriorAcpDeletion'},
    @{name='rich-embedded';suite='Accessibility';skipBuild=$true;tests='TestRichTextAccessibilityEmbeddedInteriorAcpDeletion'})
@{classification='bounded-before-implementation-functional-and-timing-diagnostic';slots=$slots;overallSeconds=1200;childSeconds=600;startedUtc=$started.ToString('o')} |
    ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'before-plan.receipt.txt') -Encoding utf8
$results=[Collections.Generic.List[object]]::new()
try {
    foreach($slot in $slots) {
        $budget=[Math]::Min(600,[Math]::Floor(1200-([DateTime]::UtcNow-$started).TotalSeconds))
        if($budget -le 0){throw 'Before-run overall deadline exhausted.'}
        $directory=Join-Path $OutputDirectory ('Before/'+$slot.name)
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
        $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true;$start.WorkingDirectory=$Root
        foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'test.ps1'),'-Configuration','Release','-Platform','x64','-SkipTooling','-Suites',$slot.suite,'-Tests',$slot.tests)){$start.ArgumentList.Add($argument)}
        if($slot.skipBuild){$start.ArgumentList.Add('-SkipBuild')}
        Write-Host "Before slot: $($slot.name)"
        $child=[Diagnostics.Process]::Start($start)
        try {
            $stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync()
            $timedOut=-not $child.WaitForExit([int]($budget*1000))
            if($timedOut){$child.Kill($true);[void]$child.WaitForExit(5000)}
            $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
            if($drained){[IO.File]::WriteAllText((Join-Path $directory 'runner.log'),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $directory 'runner.err'),$stderr.GetAwaiter().GetResult())}
            $code=if($child.HasExited){$child.ExitCode}else{$null}
            $results.Add(@{name=$slot.name;exitCode=$code;timedOut=$timedOut;drained=$drained;processId=$child.Id;completedUtc=[DateTime]::UtcNow.ToString('o')})
        } finally {$child.Dispose()}
        # Copy each failure before the stock runner reuses its filtered pathname.
        foreach($pair in @(@(".build/reports/$($slot.suite)-x64-Release.filtered.json",'native.receipt.txt'),@(".build/logs/test-$($slot.suite)-x64-Release.filtered.log",'native.log'))) {
            $from=Join-Path $Root $pair[0]
            if(Test-Path -LiteralPath $from){Copy-Item -LiteralPath $from -Destination (Join-Path $directory $pair[1])}
        }
        if($timedOut -or -not $drained -or $code -ne 1){throw "Before slot did not produce the expected bounded native failure: $($slot.name)"}
        if(-not (Test-Path -LiteralPath (Join-Path $directory 'native.receipt.txt'))){throw 'Native failure receipt missing.'}
    }
} finally {
    @{scheduled=3;attempted=$results.Count;elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()} |
        ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'before-completion.receipt.txt') -Encoding utf8
}
