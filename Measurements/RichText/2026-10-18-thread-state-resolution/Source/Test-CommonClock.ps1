[CmdletBinding()]param([string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class DxUiOwnedClockProbe {
 [DllImport("kernel32.dll", ExactSpelling=true)] public static extern bool QueryPerformanceCounter(out long value);
 [DllImport("kernel32.dll", ExactSpelling=true)] public static extern bool QueryPerformanceFrequency(out long value);
}
"@
[long]$frequency=0
if(-not[DxUiOwnedClockProbe]::QueryPerformanceFrequency([ref]$frequency) -or $frequency -ne [Diagnostics.Stopwatch]::Frequency -or -not[Diagnostics.Stopwatch]::IsHighResolution){throw 'Native/managed clock frequencies differ.'}
$rows=@(for($index=0;$index -lt 32;++$index){
 [long]$before=0;[long]$after=0
 if(-not[DxUiOwnedClockProbe]::QueryPerformanceCounter([ref]$before)){throw 'Native pre-read failed.'}
 $managed=[Diagnostics.Stopwatch]::GetTimestamp()
 if(-not[DxUiOwnedClockProbe]::QueryPerformanceCounter([ref]$after)){throw 'Native post-read failed.'}
 if($managed -lt $before -or $managed -gt $after){throw 'Managed timestamp falls outside its native QPC bracket.'}
 @{nativeBeforeQpc=$before;managedTimestamp=$managed;nativeAfterQpc=$after}
})
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
@{classification='Functional native/managed absolute-clock correlation; no clock setting, timer-resolution, priority, affinity or policy change';
 nativeTicksPerSecond=$frequency;managedTicksPerSecond=[Diagnostics.Stopwatch]::Frequency;managedHighResolution=[Diagnostics.Stopwatch]::IsHighResolution;
 runtime=[Environment]::Version.ToString();samples=$rows;scriptSha256=(Get-FileHash $PSCommandPath).Hash}|
 ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'common-clock-tests.receipt.txt')
Write-Host '32 managed timestamps fall inside native QPC brackets at identical frequencies.'
