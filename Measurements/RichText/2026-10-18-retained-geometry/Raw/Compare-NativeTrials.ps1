[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Get-Method([string]$Path,[string]$Name){
 $text=[IO.File]::ReadAllText($Path);$match=[regex]::Match($text,'void '+[regex]::Escape($Name)+'\(')
 if(-not$match.Success){throw 'Method missing.'};$first=$text.IndexOf('{',$match.Index);$depth=0
 for($index=$first;$index -lt $text.Length;++$index){
  if($text[$index] -eq '{'){++$depth}elseif($text[$index] -eq '}'){if(--$depth -eq 0){return $text.Substring($match.Index,$index+1-$match.Index)}}
 }
 throw 'Unbalanced method.'
}
function Get-Trials([string]$Log){
 $rows=@(foreach($line in Get-Content -LiteralPath $Log){
  if($line -match '^rich retained preparation trial=(\d+) sourceUnits=26624 edits=128 initialMs=([\d.]+) editMs=([\d.]+)$'){
   @{trial=[int]$Matches[1];initialMilliseconds=[double]::Parse($Matches[2],[Globalization.CultureInfo]::InvariantCulture);
    editMilliseconds=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)}
  }
 })
 if($rows.Count -ne 5 -or (@($rows.trial|Sort-Object)-join ',') -cne '0,1,2,3,4'){throw 'Fixed trial inventory differs.'}
 return @{samples=$rows;initialMedianMilliseconds=@($rows.initialMilliseconds|Sort-Object)[2];editMedianMilliseconds=@($rows.editMilliseconds|Sort-Object)[2]}
}
$work=Join-Path $Root '.build/reports/rich-retained-geometry-20261018'
$before=Join-Path $work 'Before/Source/Tests/Controls/Scope.Tests.RichTextEditor.cpp'
$after=Join-Path $Root 'Tests/Controls/Scope.Tests.RichTextEditor.cpp'
$method='TestRichTextEditorRetainedPreparationFixedTrials'
$body=Get-Method $before $method
if($body -cne (Get-Method $after $method)){throw 'Measured method changed.'}
$beforeLog=Join-Path $work 'Before/before-native.log';$afterLog=Join-Path $Root '.build/logs/test-EditorControls-x64-Release.log'
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
@{classification='Descriptive native preparation/raster observations; no same-binary control or frame-performance acceptance';
 method=$method;methodSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($body)));
 beforeLogSha256=(Get-FileHash $beforeLog).Hash;afterLogSha256=(Get-FileHash $afterLog).Hash;
 before=(Get-Trials $beforeLog);after=(Get-Trials $afterLog);
 limitations='The guarded default-font fixture uses whole layout and reports that work. The rejected unguarded draft had geometry and pixel failures and is not substituted as the candidate. No quiet-host or causality claim.'}|
 ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'native-trials.receipt.txt')
Write-Host 'Unchanged fixed method and all ten original observations verified.'
