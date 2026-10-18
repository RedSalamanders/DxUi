Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class OwnedThreadIdentity {
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
}
"@
$identity=@{threadId=[OwnedThreadIdentity]::GetCurrentThreadId();creation100ns=(Get-Process -Id $PID).Threads|Where-Object Id -EQ ([OwnedThreadIdentity]::GetCurrentThreadId())|ForEach-Object {$_.StartTime.ToFileTimeUtc()}}
$identity|ConvertTo-Json -Compress
[Threading.Thread]::Sleep(500)
$clock=[Diagnostics.Stopwatch]::StartNew()
while($clock.ElapsedMilliseconds -lt 500){[void][Diagnostics.Stopwatch]::GetTimestamp()}
