[CmdletBinding()]param([string]$OutputPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputPath){throw 'Fresh ambient snapshot required.'}
$started=[datetime]::UtcNow;$rows=[Collections.Generic.List[object]]::new();$errors=[Collections.Generic.List[object]]::new()
foreach($name in @('cl','link','MSBuild','DxUi.ControlTests','DxUi.EmbeddedTests')){
 foreach($process in [Diagnostics.Process]::GetProcessesByName($name)){
  try{
   $process.Refresh()
   $rows.Add(@{name=$name;processId=$process.Id;createdUtc=$process.StartTime.ToUniversalTime().ToString('o');cpuSeconds=$process.TotalProcessorTime.TotalSeconds})
  }catch [System.InvalidOperationException]{$errors.Add(@{name=$name;error='Process exited during the read.'})
  }catch [System.ComponentModel.Win32Exception]{$errors.Add(@{name=$name;error='Process counter access failed.';code=$_.Exception.NativeErrorCode})
  }finally{$process.Dispose()}
 }
}
@{classification='Read-only selected-image counters outside the complete native study; no frame attribution, ownership inference or quiet-host proof';
 startedUtc=$started.ToString('o');endedUtc=[datetime]::UtcNow.ToString('o');rows=$rows.ToArray();errors=$errors.ToArray();
 sourceSha256=(Get-FileHash $PSCommandPath).Hash;excluded='No command lines, paths, thread snapshots, process suspension, priority/affinity changes or process termination.'}|
 ConvertTo-Json -Depth 12|Set-Content -LiteralPath $OutputPath
Write-Host "Retained $($rows.Count) selected-image counter rows outside native frame windows."
