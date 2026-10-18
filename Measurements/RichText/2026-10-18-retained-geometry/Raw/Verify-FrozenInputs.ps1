[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$manifest=Get-Content (Join-Path $OutputDirectory 'qualified-source.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 16
$matrix=Get-Content (Join-Path $OutputDirectory 'qualification-completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 16
if($matrix.completed -ne 6 -or $matrix.attemptedOperations -ne 12){throw 'Matrix incomplete; preserve frozen inputs.'}
foreach($entry in $manifest.files){if((Get-FileHash -LiteralPath (Join-Path $Root $entry.path)).Hash -cne $entry.sha256){throw "Frozen input changed: $($entry.path)"}}
$paths=@(git -C $Root diff --name-only;git -C $Root ls-files --others --exclude-standard)|Sort-Object -Unique
if(($paths -join "`n") -cne ((@($manifest.files.path)|Sort-Object -Unique)-join "`n")){throw 'Working input path inventory changed.'}
@{classification='Exact start/end identity of all changed nonignored source/test/project/spec/doc inputs across the declared fixed matrix';
 baseCommit=$manifest.baseCommit;fileCount=$manifest.files.Count;manifestSha256=(Get-FileHash (Join-Path $OutputDirectory 'qualified-source.receipt.txt')).Hash;
 matrixSha256=(Get-FileHash (Join-Path $OutputDirectory 'qualification-completion.receipt.txt')).Hash;verifiedUtc=[datetime]::UtcNow.ToString('o')}|
 ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'frozen-inputs-end.receipt.txt')
Write-Host 'Frozen source, tests, projects, specs and docs match their prelaunch identities.'
