[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-source-changes-20261018'
function Read-Receipt([string]$Path){Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Read-Trials([string]$Path){
 $edits=@();$backward=@();$lookup=@()
 foreach($line in Get-Content -LiteralPath $Path){
  if($line -match '^RichDocument edit trial=(\d+) edits=4096 sourceUnits=12288 elapsedMs=([0-9.]+)$'){
   $edits+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^UIA single-backward trial=(\d+) sourceUnits=61440 queries=32 elapsedMs=([0-9.]+)$'){
   $backward+=@{trial=[int]$Matches[1];elapsedMs=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture)}
  }elseif($line -match '^RichDocument fragmented lookup diagnostic: insertions=4096 units=24580 queries=8192 expected=5431 documentBytes=88 snapshotBytes=16 trialMs=(.+)$'){
   $lookup=@($Matches[1].TrimEnd(',').Split(',')|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
  }
 }
 if($edits.Count -ne 5 -or $backward.Count -ne 5 -or $lookup.Count -ne 5){throw 'Fixed five-trial schedule differs.'}
 return @{edit=$edits;backward=$backward;lookup=$lookup;editMedianMs=@($edits.elapsedMs|Sort-Object)[2];backwardMedianMs=@($backward.elapsedMs|Sort-Object)[2];lookupMedianMs=@($lookup|Sort-Object)[2]}
}
$before=Read-Receipt (Join-Path $work 'before-native.receipt.txt')
$after=Read-Receipt (Join-Path $Root '.build/reports/EditorControls-x64-Release.json')
$beforeReport=Read-Receipt $before.performanceReport
$afterReport=Read-Receipt $after.performanceReport
@{classification='descriptive-within-process-operation-trials';beforeSource=$beforeReport.sourceFingerprint;afterSource=$afterReport.sourceFingerprint;
 beforeArtifact=$before.sha256;afterArtifact=$after.sha256;beforePerformanceReport=$before.performanceReport;afterPerformanceReport=$after.performanceReport;
 before=(Read-Trials (Join-Path $work 'before-native.log'));after=(Read-Trials (Join-Path $Root '.build/logs/test-EditorControls-x64-Release.log'));
 limitations='One fresh native process per source, five fixed trials inside each. The baseline expected-fails a new capability oracle after its timing trials. No same-binary controls, quiet-host proof or timing acceptance. Resource limits and measured bodies are unchanged.'} |
 ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $work 'trial-analysis.receipt.txt') -Encoding utf8
function Get-Method([string]$File,[string]$Name){
 $text=[IO.File]::ReadAllText($File)
 $match=[regex]::Match($text,'(?:static )?void '+[regex]::Escape($Name)+'\(')
 if(-not $match.Success){throw "Method missing: $Name"}
 $first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}
  elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced method span.'
}
foreach($method in (Read-Receipt (Join-Path $work 'unchanged-measured-methods.receipt.txt'))){
 $reference=if($method.beforePrefix -eq 'Raw/BeforeSource/'){Join-Path $work ('BeforeSource/'+$method.file)}else{Join-Path $work ('PriorCommittedSource/'+$method.file)}
 foreach($file in @($reference,(Join-Path $Root $method.file))){
  $body=Get-Method $file $method.method
  if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body))) -cne $method.sha256){throw 'A measured oracle/budget method changed.'}
 }
}
Write-Host 'Five measured method bodies match their exact retained reference copies. Derived descriptive trials retained.'
