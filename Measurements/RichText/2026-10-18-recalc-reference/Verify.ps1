[CmdletBinding()] param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
foreach($line in Get-Content -LiteralPath (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 $target=[IO.Path]::GetFullPath((Join-Path $Packet $Matches[2]))
 if(-not$target.StartsWith(([IO.Path]::GetFullPath($Packet)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase) -or
  (Get-FileHash -LiteralPath $target).Hash -cne $Matches[1]){throw 'Reference seal differs.'}
}
foreach($copy in (Read-Receipt 'inventory.receipt.txt').copies){
 if((Get-FileHash -LiteralPath (Join-Path $Packet $copy.path)).Hash -cne $copy.sha256){throw 'Original-to-copy reference hash differs.'}
}
$source=Read-Receipt 'Raw/office-recalc/inventory.receipt.txt'
if($source.sha256 -cne '7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41' -or
 $source.programChanged -or $source.activeBefore -cne '/windows-edit/user32.dll' -or $source.activeAfter -cne $source.activeBefore -or
 $source.programPath -cne '/office-modern/RICHED20.DLL' -or $source.imageBase -cne '180000000' -or $source.functions.Count -ne 6){throw 'Pinned program/read-only identity differs.'}
$expected=@{'1800040b0'='CDisplayML::StepBackgroundRecalc';'180040810'='CDisplayML::WaitForRecalc';'180040990'='CDisplayEngineML::WaitForRecalc';
 '18003df50'='CDisplayEngineML::RecalcLines';'180041280'='CDisplayML::RecalcView';'180045170'='CDisplay::InvalidateRecalc'}
foreach($function in $source.functions){
 if($expected[$function.entry] -cne $function.name -or $function.bodyBytes -le 0){throw 'Function identity differs.'}
 $path=Join-Path $Packet ('Raw/office-recalc/'+$function.file)
 if((Get-Item -LiteralPath $path).Length -le 0 -or -not(Select-String -LiteralPath $path -SimpleMatch $function.name)){throw 'Actual named pseudocode is missing.'}
}
$mcp=Read-Receipt 'Raw/office-recalc-mcp.receipt.txt'
$payload=$mcp.content[0].text|ConvertFrom-Json -AsHashtable
if($mcp.isError -or -not$payload.success -or -not$payload.console_output.Contains('Verified six saved Office recalculation functions') -or
 -not$payload.console_output.Contains('=== SCRIPT COMPLETED SUCCESSFULLY ===')){throw 'Actual MCP Java execution did not succeed.'}
$process=Read-Receipt 'Raw/office-recalc-process.receipt.txt'
if($process.exitCode -ne 0 -or $process.timedOut -or -not$process.drained -or $process.elapsedSeconds -gt 180){throw 'Owned MCP collection did not complete in budget.'}
Write-Host 'Verified six actual MCP decompilations, pinned Office identity, read-only state, unchanged active program and exact evidence hashes.'
Write-Host 'Private bit/structure interpretation and production behavior remain separate from this evidence collection.'
