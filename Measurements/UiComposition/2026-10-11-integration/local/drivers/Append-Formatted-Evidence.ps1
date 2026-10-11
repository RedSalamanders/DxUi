[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$packet = Join-Path $root 'Measurements/UiComposition/2026-10-11-integration'
$bindings = [Collections.Generic.List[object]]::new()
$map = [ordered]@{
 'qualified-debug-formatted'='formatted-debug'; 'qualified-release-formatted'='formatted-release'; 'qualified-asan-formatted'='formatted-asan';
 'focus-integration-Debug-attempt4'='formatted-focus-debug'; 'focus-integration-Release-attempt4'='formatted-focus-release'; 'focus-integration-ASanDebug-attempt4'='formatted-focus-asan'
}
foreach ($entry in $map.GetEnumerator()) {
 $source = Join-Path $PSScriptRoot $entry.Key
 $destination = Join-Path $packet ('local/'+$entry.Value)
 if (Test-Path -LiteralPath $destination) { throw 'Preserve the earlier formatted evidence.' }
 New-Item -ItemType Directory -Path $destination | Out-Null
 foreach ($file in @(Get-ChildItem -LiteralPath $source -File)) {
  $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
  $copy = Join-Path $destination $file.Name
  Copy-Item -LiteralPath $file.FullName -Destination $copy
  if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $hash -or (Get-Item -LiteralPath $copy).Length -ne $file.Length) { throw 'Changed formatted evidence bytes.' }
  $bindings.Add([ordered]@{original=$file.FullName; copy=('local/'+$entry.Value+'/'+$file.Name); bytes=$file.Length; sha256=$hash})
 }
 $report = Get-Content -LiteralPath (Join-Path $destination 'canonical-performance.json') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
 $scope = if ($entry.Value -like 'formatted-focus-*') { 'Four full foreground suites passed with zero skips and verified restoration.' } else { 'Fourteen selected form/quiet cases and six embedded client cases passed with zero skips.' }
 $prose = "# $($entry.Value)`n`n$scope Native source: $($report.sourceCommit), $($report.configuration) $($report.platform).`n`nThe canonical complex-UI report is an unpaired synthetic observation. Actual source, compiler/SDK, dependency, artifact and environment identities are retained before and after execution. It establishes no paired non-regression or benchmark-policy acceptance. See the [integration explanation](../../README.md) for scope and remaining gates.`n"
 [IO.File]::WriteAllText((Join-Path $destination 'README.md'),$prose,[Text.UTF8Encoding]::new($false))
}
foreach ($name in @('Invoke-Formatted-Local.ps1','Append-Formatted-Evidence.ps1','Assert-Formatted-Archival-Inputs.ps1')) {
 $source = Join-Path $PSScriptRoot $name
 $copy = Join-Path $packet ('local/drivers/'+$name)
 $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
 Copy-Item -LiteralPath $source -Destination $copy
 if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $hash) { throw 'Changed formatted driver bytes.' }
 $bindings.Add([ordered]@{original=$source; copy=('local/drivers/'+$name); bytes=(Get-Item -LiteralPath $source).Length; sha256=$hash})
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'formatted-native-compiled-closure.receipt.txt') -Destination (Join-Path $packet 'formatted-inputs-before.receipt.txt')
foreach ($pair in @(@('.build/reports/AddressSanitizer-x64.json','local/formatted-asan/detection.receipt.txt'),
                    @('.build/logs/test-AddressSanitizer-x64.log','local/formatted-asan/detection.log'))) {
 $source = Join-Path $root $pair[0]
 $copy = Join-Path $packet $pair[1]
 $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
 Copy-Item -LiteralPath $source -Destination $copy
 if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -cne $hash) { throw 'Changed formatted sanitizer probe bytes.' }
 $bindings.Add([ordered]@{original=$source; copy=$pair[1]; bytes=(Get-Item -LiteralPath $source).Length; sha256=$hash})
}
[IO.File]::WriteAllText((Join-Path $packet 'formatted-copy.receipt.txt'),((ConvertTo-Json -InputObject $bindings.ToArray() -Depth 12)+[char]10),[Text.UTF8Encoding]::new($false))
Write-Output "Retained $($bindings.Count) byte-verified formatted evidence bindings."
