[CmdletBinding()] param([string] $Root,[string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[DateTime]::UtcNow
$slots=@(
    @{name='edit-read-single-step';exe=(Get-Command pwsh).Source;expected=0;arguments=@('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'test.ps1'),'-Platform','x64','-Configuration','Release','-SkipTooling','-Suites','EditorControls','-Tests','TestRichDocumentFragmentedSnapshotOracle,TestRichDocumentFragmentedEditFixedTrials,TestTextElementSingleBackwardFixedTrials')},
    @{name='retained';exe=(Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe');expected=1;arguments=@('--test-rich-document-storage','Retained')},
    @{name='same-content';exe=(Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe');expected=1;arguments=@('--test-rich-document-storage','SameContent')})
@{classification='before-storage-change-fixed-resource-budgets-and-descriptive-trials';slots=$slots;overallSeconds=1200;childSeconds=600;startedUtc=$started.ToString('o');retainedCppByteBudget=4194304;noOpCppAllocationBudget=0} |
 ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'before-plan.receipt.txt') -Encoding utf8
$results=[Collections.Generic.List[object]]::new()
try{
 foreach($slot in $slots){
    $budget=[Math]::Min(600,[Math]::Floor(1200-([DateTime]::UtcNow-$started).TotalSeconds))
    if($budget -le 0){throw 'Before-run deadline exhausted.'}
    $directory=Join-Path $OutputDirectory ('Before/'+$slot.name)
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $start=[Diagnostics.ProcessStartInfo]::new($slot.exe)
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true;$start.WorkingDirectory=$Root
    foreach($argument in $slot.arguments){$start.ArgumentList.Add($argument)}
    Write-Host "Before slot: $($slot.name)"
    $child=[Diagnostics.Process]::Start($start)
    try{
      $stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync()
      $timedOut=-not $child.WaitForExit([int]($budget*1000))
      if($timedOut){$child.Kill($true);[void]$child.WaitForExit(5000)}
      $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
      if($drained){[IO.File]::WriteAllText((Join-Path $directory 'runner.log'),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $directory 'runner.err'),$stderr.GetAwaiter().GetResult())}
      $code=if($child.HasExited){$child.ExitCode}else{$null}
      $result=@{name=$slot.name;exitCode=$code;timedOut=$timedOut;drained=$drained;processId=$child.Id;completedUtc=[DateTime]::UtcNow.ToString('o')}
      if($slot.name -ne 'edit-read-single-step'){
        $result.executable=$slot.exe;$result.sha256=(Get-FileHash -LiteralPath $slot.exe).Hash
        $build=Get-Content (Join-Path $Root '.build/reports/performance-build/x64-Release/build.json') -Raw|ConvertFrom-Json
        $result.buildAttestationIdentity=$build.identity
      }
      $results.Add($result)
      $result|ConvertTo-Json -Depth 12|Set-Content (Join-Path $directory 'process.receipt.txt') -Encoding utf8
    }finally{$child.Dispose()}
    if($slot.name -eq 'edit-read-single-step'){
      Copy-Item -LiteralPath (Join-Path $Root '.build/reports/EditorControls-x64-Release.filtered.json') -Destination (Join-Path $directory 'native.receipt.txt')
      Copy-Item -LiteralPath (Join-Path $Root '.build/logs/test-EditorControls-x64-Release.filtered.log') -Destination (Join-Path $directory 'native.log')
      foreach($kind in @('build','incremental')){Copy-Item -LiteralPath (Join-Path $Root ".build/reports/performance-build/x64-Release/$kind.json") -Destination (Join-Path $directory "$kind.receipt.txt")}
    }
    if($timedOut -or -not $drained -or $code -ne $slot.expected){throw "Unexpected before outcome: $($slot.name), exit=$code"}
 }
}finally{
 @{scheduled=$slots.Count;attempted=$results.Count;elapsedSeconds=([DateTime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()}|
  ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'before-completion.receipt.txt') -Encoding utf8
}
