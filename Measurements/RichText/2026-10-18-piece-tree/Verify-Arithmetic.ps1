[CmdletBinding()] param([string] $Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
& (Join-Path $Packet 'Verify.ps1') -Packet $Packet
function Read-Receipt([string]$Path){Get-Content -LiteralPath (Join-Path $Packet $Path) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
$analysis=Read-Receipt 'Raw/trial-analysis.receipt.txt'
$native=Read-Receipt 'Native/EditorControls-x64-Release.receipt.txt'
$inventory=Read-Receipt 'inventory.receipt.txt'
if($analysis.afterSource -cne $inventory.sourceFingerprint -or $analysis.afterArtifact -cne $native.sha256 -or $analysis.afterPerformanceReport -cne $native.performanceReport){throw 'Derived trial source/artifact binding differs.'}
foreach($pair in @(@('before','Raw/Before/edit-read-single-step/native.log'),@('after','Native/EditorControls-x64-Release.log'))){
 $edits=@();$backward=@();$lookup=@()
 foreach($line in Get-Content -LiteralPath (Join-Path $Packet $pair[1])){
  if($line -match '^RichDocument edit trial=(\d+) edits=4096 sourceUnits=12288 elapsedMs=([0-9.]+)$'){
   $edits+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^UIA single-backward trial=(\d+) sourceUnits=61440 queries=32 elapsedMs=([0-9.]+)$'){
   $backward+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^RichDocument fragmented lookup diagnostic: insertions=4096 units=24580 queries=8192 expected=5431 documentBytes=88 snapshotBytes=16 trialMs=(.+)$'){
   $lookup=@($Matches[1].TrimEnd(',').Split(',')|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
  }
 }
 $value=$analysis[$pair[0]]
 if($edits.Count -ne 5 -or $backward.Count -ne 5 -or $lookup.Count -ne 5){throw 'Five fixed trials not present in the original log.'}
 for($index=0;$index -lt 5;++$index){
  foreach($field in @('trial','elapsedMs')){
   if($edits[$index][$field] -ne $value.edit[$index][$field] -or $backward[$index][$field] -ne $value.backward[$index][$field]){throw 'Derived operation trial differs.'}
  }
  if($lookup[$index] -ne $value.lookup[$index]){throw 'Derived lookup trial differs.'}
 }
 if(@($edits.elapsedMs|Sort-Object)[2] -ne $value.editMedianMs -or @($backward.elapsedMs|Sort-Object)[2] -ne $value.backwardMedianMs -or
    @($lookup|Sort-Object)[2] -ne $value.lookupMedianMs){throw 'Derived descriptive median differs.'}
}
Write-Host 'Recomputed all 30 descriptive observations and six medians from the original before/final Release logs.'
