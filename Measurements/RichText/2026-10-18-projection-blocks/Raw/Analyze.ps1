[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-incremental-preparation-20261018'
function Read-Receipt([string]$File){Get-Content -LiteralPath $File -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Read-Trials([string]$Path,[bool]$HasWhole){
 $value=@{blockInitial=@();blockEdits=@();wholeInitial=@();wholeEdits=@()}
 foreach($line in Get-Content -LiteralPath $Path){
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
  $expected=if($HasWhole -or $field.StartsWith('block')){5}else{0}
  if($value[$field].Count -ne $expected){throw "Fixed trial schedule differs: $Path / $field"}
  if($expected){$value[$field+'MedianMs']=@($value[$field].elapsedMs|Sort-Object)[2]}
 }
 return $value
}
$entries=@{firstDraft='before-native.receipt.txt';before='BeforeRefined/before-native.receipt.txt'}
$bindings=@{}
foreach($entry in $entries.GetEnumerator()){
 $native=Read-Receipt (Join-Path $work $entry.Value)
 $report=Read-Receipt $native.performanceReport
 $bindings[$entry.Key]=@{sourceFingerprint=$report.sourceFingerprint;nativeSha256=$native.sha256;performanceReport=$native.performanceReport}
}
$native=Read-Receipt (Join-Path $Root '.build/reports/EditorControls-x64-Release.json')
$report=Read-Receipt $native.performanceReport
$bindings.after=@{sourceFingerprint=$report.sourceFingerprint;nativeSha256=$native.sha256;performanceReport=$native.performanceReport}
@{classification='descriptive-fixed-projection-operation-trials';bindings=$bindings;
 firstDraft=(Read-Trials (Join-Path $work 'before-native.log') $false);
 before=(Read-Trials (Join-Path $work 'BeforeRefined/before-native.log') $true);
 after=(Read-Trials (Join-Path $Root '.build/logs/test-EditorControls-x64-Release.log') $true);
 limitations='One fresh native process per source with five trials within it. Initial draft copy overhead was removed and the before baseline recollected before reuse activation. Source slicing construction and adjacent payload sharing also change in the final candidate. No same-binary controls, quiet-host proof, sole-cause or completed-frame acceptance.'}|
 ConvertTo-Json -Depth 12|Set-Content (Join-Path $work 'trial-analysis.receipt.txt')
function Get-Method([string]$File,[string]$Name){
 $text=[IO.File]::ReadAllText($File);$match=[regex]::Match($text,'void '+[regex]::Escape($Name)+'\(')
 if(-not$match.Success){throw "Method missing: $Name"}
 $first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}
  elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced method.'
}
$methods=@();$relative='Tests/Controls/Scope.Tests.RichProjectionBlocks.cpp'
foreach($name in @('TestRichProjectionBlockFixedTrials','TestRichProjectionWholeSourceFixedTrials')){
 $original=Get-Method (Join-Path $work ('BeforeRefined/BeforeSource/'+$relative)) $name
 $current=Get-Method (Join-Path $Root $relative) $name
 if($original -cne $current){throw 'A refined before/final measured method changed.'}
 $methods+=@{method=$name;file=$relative;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($current)))}
}
ConvertTo-Json -InputObject $methods -Depth 6|Set-Content (Join-Path $work 'unchanged-measured-methods.receipt.txt')
Write-Host 'Retained 50 descriptive observations and ten medians; both refined-before/final measured method bodies match.'
