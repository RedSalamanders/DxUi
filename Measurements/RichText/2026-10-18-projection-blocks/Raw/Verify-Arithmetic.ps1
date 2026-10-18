[CmdletBinding()] param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
& (Join-Path $Packet 'Verify.ps1') -Packet $Packet
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
$analysis=Read-Receipt 'Raw/trial-analysis.receipt.txt'
$inventory=Read-Receipt 'inventory.receipt.txt'
foreach($entry in @(@('firstDraft','Raw/before-native.receipt.txt','Raw/before-native.log'),
 @('before','Raw/BeforeRefined/before-native.receipt.txt','Raw/BeforeRefined/before-native.log'),
 @('after','Native/EditorControls-x64-Release.receipt.txt','Native/EditorControls-x64-Release.log'))){
 $native=Read-Receipt $entry[1]
 $report=Read-Receipt ('Performance/'+(Split-Path $native.performanceReport -Leaf))
 $binding=$analysis.bindings[$entry[0]]
 if($binding.nativeSha256 -cne $native.sha256 -or $binding.performanceReport -cne $native.performanceReport -or
  $binding.sourceFingerprint -cne $report.sourceFingerprint){throw 'Derived source/artifact binding differs.'}
 $value=@{blockInitial=@();blockEdits=@();wholeInitial=@();wholeEdits=@()}
 foreach($line in Get-Content -LiteralPath (Join-Path $Packet $entry[2])){
  if($line -match '^RichProjection initial trial=(\d+) sourceUnits=30634 blocks=1024 elapsedMs=([0-9.]+)$'){
   $value.blockInitial+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^RichProjection edits trial=(\d+) edits=128 rebuiltSourceUnits=(\d+) reusedBlocks=(\d+) elapsedMs=([0-9.]+)$'){
   $value.blockEdits+=@{trial=[int]$Matches[1];rebuiltSourceUnits=[int]$Matches[2];reusedBlocks=[int]$Matches[3];elapsedMs=[double]::Parse($Matches[4],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^RichProjection whole-initial trial=(\d+) sourceUnits=30634 elapsedMs=([0-9.]+)$'){
   $value.wholeInitial+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^RichProjection whole-edits trial=(\d+) edits=128 checksum=3528064 elapsedMs=([0-9.]+)$'){
   $value.wholeEdits+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }
 }
 foreach($field in @('blockInitial','blockEdits','wholeInitial','wholeEdits')){
  $expected=if($entry[0] -cne 'firstDraft' -or $field.StartsWith('block')){5}else{0}
  if($value[$field].Count -ne $expected -or $analysis[$entry[0]][$field].Count -ne $expected){throw 'Fixed five-trial schedule differs.'}
  for($index=0;$index -lt $expected;++$index){
   foreach($property in $value[$field][$index].Keys){
    if($value[$field][$index][$property] -ne $analysis[$entry[0]][$field][$index][$property]){throw 'Derived observation differs.'}
   }
  }
  if($expected -and @($value[$field].elapsedMs|Sort-Object)[2] -ne $analysis[$entry[0]][$field+'MedianMs']){throw 'Derived descriptive median differs.'}
 }
 foreach($trial in $value.blockEdits){
  $units=if($entry[0] -ceq 'after'){11520}else{3921152}
  $reused=if($entry[0] -ceq 'after'){130688}else{0}
  if($trial.rebuiltSourceUnits -ne $units -or $trial.reusedBlocks -ne $reused){throw 'Rebuilt source/reused block observation differs.'}
 }
}
if($analysis.bindings.after.sourceFingerprint -cne $inventory.sourceFingerprint){throw 'Derived final source differs.'}
Write-Host 'Recomputed all 50 original operation observations, ten medians and rebuilt/reused work counts. No frame/timing acceptance is implied.'
