[CmdletBinding()] param([string] $Root,[string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Parse-Double([string]$Value){[double]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture)}
function Read-Trials([string]$File){
 $edit=[Collections.Generic.List[object]]::new();$back=[Collections.Generic.List[object]]::new();$lookup=@()
 foreach($line in Get-Content -LiteralPath $File){
  if($line -match '^RichDocument edit trial=(\d+) edits=4096 sourceUnits=12288 elapsedMs=([0-9.]+)$'){
   $edit.Add(@{trial=[int]$Matches[1];elapsedMs=(Parse-Double $Matches[2])})
  }elseif($line -match '^UIA single-backward trial=(\d+) sourceUnits=61440 queries=32 elapsedMs=([0-9.]+)$'){
   $back.Add(@{trial=[int]$Matches[1];elapsedMs=(Parse-Double $Matches[2])})
  }elseif($line -match '^RichDocument fragmented lookup diagnostic: insertions=4096 units=24580 queries=8192 expected=5431 documentBytes=88 snapshotBytes=16 trialMs=([0-9.,]+)$'){
   $lookup=@($Matches[1].TrimEnd(',').Split(',')|ForEach-Object{Parse-Double $_})
  }
 }
 if($edit.Count -ne 5 -or $back.Count -ne 5 -or $lookup.Count -ne 5){throw 'Fixed descriptive trial inventory differs.'}
 @{edit=$edit.ToArray();backward=$back.ToArray();lookup=$lookup;editMedianMs=@($edit.elapsedMs|Sort-Object)[2];backwardMedianMs=@($back.elapsedMs|Sort-Object)[2];lookupMedianMs=@($lookup|Sort-Object)[2]}
}
$before=Read-Trials (Join-Path $OutputDirectory 'Before/edit-read-single-step/native.log')
$after=Read-Trials (Join-Path $Root '.build/logs/test-EditorControls-x64-Release.log')
$native=Get-Content (Join-Path $Root '.build/reports/EditorControls-x64-Release.json') -Raw|ConvertFrom-Json
$performance=Get-Content -LiteralPath $native.performanceReport -Raw|ConvertFrom-Json
if($native.exitCode -ne 0 -or $native.skips.Count -or $performance.sourceFingerprint -cne '8EE9BD21B0CE810E1B6B39EBE30D00EF62945764CDF52D8C9AB8D65D8B8EFD99'){throw 'After trials use an obsolete or unsuccessful native artifact.'}
@{classification='descriptive-one-native-process-per-source-with-five-within-process-trials';
 beforeSource='6B5ED058F3E2B3E5C2EB479A7EEDE48D615CBAAB3B98DEDE229F41B355DF1B71';afterSource=$performance.sourceFingerprint;
 before=$before;after=$after;afterArtifact=$native.sha256;afterPerformanceReport=$native.performanceReport;
 limitations='No same-binary controls or quiet-host proof; the dormant fault-injection hook is added after the before run. These are operation diagnostics, not Default completed-frame acceptance.'}|
 ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'trial-analysis.receipt.txt') -Encoding utf8
[pscustomobject]@{beforeEdit=$before.editMedianMs;afterEdit=$after.editMedianMs;beforeLookup=$before.lookupMedianMs;afterLookup=$after.lookupMedianMs;beforeBackward=$before.backwardMedianMs;afterBackward=$after.backwardMedianMs}
