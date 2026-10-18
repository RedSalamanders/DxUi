[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$plan=Get-Content (Join-Path $OutputDirectory 'plan.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
$completed=Get-Content (Join-Path $OutputDirectory 'completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable
$failure=Get-Content (Join-Path $OutputDirectory 'initial-collector-failure.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable
if($failure.collectorExitCode -ne 1 -or $completed.exitCode -ne 0 -or $completed.timedOut -or -not$completed.drained){throw 'Original child did not pass before collector failure.'}
if((Get-FileHash $plan.nativeExecutable).Hash -cne $plan.nativeSha256){throw 'Original native artifact changed.'}
if((Get-FileHash (Join-Path $OutputDirectory 'initial-collector-source.ps1.receipt.txt')).Hash -cne $plan.sourceSha256){throw 'Original collector source differs from launch binding.'}
$path=Join-Path $Root '.build/reports/EditorControls-x64-Release.filtered.json'
$log=Join-Path $Root '.build/logs/test-EditorControls-x64-Release.filtered.log'
$native=Get-Content $path -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($native.exitCode -ne 0 -or $native.sha256 -cne $plan.nativeSha256 -or $native.skips.Count -or -not$native.Contains('tests') -or
 $native.tests.Count -ne 1 -or $native.tests[0] -cne 'TestRichTextEditorRetainedPreparationFixedTrials' -or
 [datetime]$native.completedUtc -lt [datetime]$plan.startedUtc){throw 'Completed filtered native receipt differs.'}
foreach($name in @('selected-native.receipt.txt','selected-native.log','native-trials.receipt.txt')){
 if(Test-Path -LiteralPath (Join-Path $OutputDirectory $name)){throw 'Preserve existing recovery outputs.'}
}
Copy-Item -LiteralPath $path -Destination (Join-Path $OutputDirectory 'selected-native.receipt.txt')
Copy-Item -LiteralPath $log -Destination (Join-Path $OutputDirectory 'selected-native.log')
Copy-Item -LiteralPath $native.performanceReport -Destination (Join-Path $OutputDirectory 'canonical-diagnostic.receipt.txt')
if(Test-Path -LiteralPath ($native.performanceReport+'.comparison.json')){
 Copy-Item -LiteralPath ($native.performanceReport+'.comparison.json') -Destination (Join-Path $OutputDirectory 'canonical-comparison.receipt.txt')
}
& (Join-Path $PSScriptRoot 'Compare-NativeTrials.ps1') -Root $Root -OutputDirectory $OutputDirectory -AfterLog (Join-Path $OutputDirectory 'selected-native.log')
@{classification='Evidence-only collector recovery; no native child rerun and no wrapper failure promoted to a pass';
 initialCollectorExitCode=1;originalRootChildExitCode=$completed.exitCode;recoveryExitCode=0;nativeRerun=$false;
 actualReceipt=$path;actualLog=$log;nativeSha256=$native.sha256;receiptSha256=(Get-FileHash $path).Hash;
 sourceSha256=(Get-FileHash $PSCommandPath).Hash;comparisonSourceSha256=(Get-FileHash (Join-Path $PSScriptRoot 'Compare-NativeTrials.ps1')).Hash}|
 ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'collector-recovery.receipt.txt')
Write-Host 'Recovered the original successful filtered receipt and five unchanged-body samples; initial collector failure retained.'
