[CmdletBinding()] param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow
$profiles=@(@{platform='x64';configuration='Release'},@{platform='x64';configuration='Debug'},@{platform='x64';configuration='ASan Debug'},
 @{platform='ARM64';configuration='Debug'},@{platform='ARM64';configuration='Release'},@{platform='ARM64';configuration='ASan Debug'})
$operations=[Collections.Generic.List[object]]::new();$completed=[Collections.Generic.List[object]]::new();$attempted=0
@{startedUtc=$started.ToString('o');profiles=$profiles;scheduled=6;scheduledOperations=12;overallBudgetSeconds=3600;profileBudgetSeconds=900;buildChildBudgetSeconds=600;probeChildBudgetSeconds=60;
 notes='All repository inputs remain frozen. Release runs all19 ordinary scopes. Debug/ASan root builds and both isolated allocation probes precede ordinary suites, so unexpected termination is caught without a CRT dialog. Their suite runners use SkipBuild with that exact attestation. ARM64 is cross-build only. Canonical frame reports are unpaired diagnostics, not acceptance.'}|
 ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'qualification-plan.receipt.txt')
function Invoke-Owned([string]$Name,[string]$Executable,[string[]]$Arguments,[int]$Limit,[datetime]$ProfileStarted,[bool]$Sanitized=$false){
 $seconds=[math]::Floor([math]::Min($Limit,[math]::Min(900-([datetime]::UtcNow-$ProfileStarted).TotalSeconds,3600-([datetime]::UtcNow-$started).TotalSeconds)))
 if($seconds -le 0){throw 'Fixed qualification deadline reached.'}
 $start=[Diagnostics.ProcessStartInfo]::new($Executable)
 $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
 if($Sanitized){$start.Environment['ASAN_OPTIONS']='halt_on_error=1:abort_on_error=0:detect_leaks=0'}
 Write-Host "Qualification: $Name"
 $process=[Diagnostics.Process]::Start($start)
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  $timedOut=-not$process.WaitForExit([int]($seconds*1000))
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.err')),$stderr.GetAwaiter().GetResult())}
  $code=if($process.HasExited){$process.ExitCode}else{$null}
  $operations.Add(@{name=$Name;processId=$process.Id;arguments=$Arguments;exitCode=$code;timedOut=$timedOut;drained=$drained;sanitizedDiagnosticOptions=$Sanitized;completedUtc=[datetime]::UtcNow.ToString('o')})
  if($timedOut -or -not$drained -or $code -ne 0){throw "Qualification failed: $Name (exit $code)"}
 }finally{$process.Dispose()}
}
try{
 foreach($profile in $profiles){
  ++$attempted;$profileStarted=[datetime]::UtcNow
  $stem=$profile.platform+'-'+$profile.configuration.Replace(' ','-')
  $base=@('-NoProfile','-NonInteractive','-File')
  $options=@('-Platform',$profile.platform,'-Configuration',$profile.configuration)
  $prebuild=$profile.platform -ceq 'x64' -and $profile.configuration -cne 'Release'
  if($prebuild){
   Invoke-Owned ($stem+'-build') (Get-Command pwsh).Source ($base+@((Join-Path $Root 'build.ps1'))+$options) 600 $profileStarted
   $buildFile=Join-Path $Root ('.build/reports/performance-build/'+$profile.platform+'-'+$profile.configuration.Replace(' ','')+'/build.json')
   $build=Get-Content -LiteralPath $buildFile -Raw|ConvertFrom-Json
   if($build.outcome -cne 'PASSED'){throw 'Probe has no successful root attestation.'}
   $executable=Join-Path $Root ('.build/x64/'+$profile.configuration+'/DxUi.EmbeddedTests.exe')
   $hash=(Get-FileHash -LiteralPath $executable).Hash
   $sanitized=$profile.configuration -ceq 'ASan Debug'
   if($sanitized -and -not(Test-Path -LiteralPath (Join-Path (Split-Path $executable) 'clang_rt.asan_dynamic-x86_64.dll'))){throw 'ASan diagnostic runtime missing; do not launch a loader-error dialog.'}
   foreach($probe in @('snapshot-slice','projection')){
    $argument=if($probe -ceq 'projection'){'--test-rich-projection-failures'}else{'--test-rich-snapshot-slice-failures'}
    $name=$stem+'-'+$probe+'-probe'
    Invoke-Owned $name $executable @($argument) 60 $profileStarted $sanitized
    if((Get-FileHash -LiteralPath $executable).Hash -cne $hash){throw 'Probe artifact changed.'}
    @{classification='isolated-successful-allocation-sweep';configuration=$profile.configuration;executable=$executable;sha256=$hash;buildAttestationIdentity=$build.identity;exitCode=0;argument=$argument}|
     ConvertTo-Json|Set-Content (Join-Path $OutputDirectory ($name+'.receipt.txt'))
   }
  }
  if($profile.platform -ceq 'x64'){
   $args=$base+@((Join-Path $Root 'test.ps1'))+$options+@('-Full','-SkipTooling')
   if($prebuild){$args+=@('-SkipBuild')}
   Invoke-Owned $stem (Get-Command pwsh).Source $args 900 $profileStarted
  }else{Invoke-Owned $stem (Get-Command pwsh).Source ($base+@((Join-Path $Root 'build.ps1'))+$options) 600 $profileStarted}
  $completed.Add(@{platform=$profile.platform;configuration=$profile.configuration;exitCode=0;elapsedSeconds=([datetime]::UtcNow-$profileStarted).TotalSeconds})
 }
}finally{
 @{scheduled=6;attempted=$attempted;completed=$completed.Count;scheduledOperations=12;attemptedOperations=$operations.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;profiles=$completed.ToArray();operations=$operations.ToArray()}|
  ConvertTo-Json -Depth 10|Set-Content (Join-Path $OutputDirectory 'qualification-completion.receipt.txt')
}
if($completed.Count -ne 6 -or $operations.Count -ne 12){throw 'Incomplete fixed qualification inventory.'}
