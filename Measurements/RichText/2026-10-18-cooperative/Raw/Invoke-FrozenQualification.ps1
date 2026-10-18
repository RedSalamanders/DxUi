[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-cooperative-20261018'
$affected=Get-Content (Join-Path $work 'HostFifth/completion.receipt.txt') -Raw|ConvertFrom-Json
if($affected.attempted -ne 2 -or @($affected.operations|Where-Object {$_.exitCode -ne 0 -or $_.timedOut -or -not$_.drained}).Count){throw 'Final affected candidate did not pass.'}
$output=Join-Path $work 'Final'
if(Test-Path -LiteralPath $output){throw 'Fresh final output required; never replace a failed matrix.'}
New-Item -ItemType Directory -Path $output|Out-Null
$paths=@(git -C $Root diff --name-only HEAD;git -C $Root ls-files --others --exclude-standard)|Sort-Object -Unique
$files=@(foreach($path in $paths){
 $from=Join-Path $Root $path;$to=Join-Path $output ('QualifiedSource/'+$path)
 New-Item -ItemType Directory -Path (Split-Path $to) -Force|Out-Null
 Copy-Item -LiteralPath $from -Destination $to
 @{path=$path;sha256=(Get-FileHash -LiteralPath $from).Hash}
})
$anchor=Get-Content (Join-Path $Root '.build/reports/rich-warp-threading-20261018/Controls/plan.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
foreach($path in $anchor.anchor.benchmarkInputs.Keys){
 if((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -cne $anchor.anchor.benchmarkInputs[$path]){throw 'Canonical benchmark input changed.'}
}
@{baseCommit=(git -C $Root rev-parse HEAD);files=$files;startedUtc=[datetime]::UtcNow.ToString('o');
 unchangedCanonicalInputs=$anchor.anchor.benchmarkInputs;unchangedCanonicalBenchmarkSha256=$anchor.anchor.benchmarkSha256;
 notes='All changed source/test/prose inputs frozen across the complete six-profile functional/resource qualification. Test-only drain helper changes mean prior fixed-trial observations are not an unchanged-fixture causal comparator. No frame acceptance or quiet-host claim.'}|
 ConvertTo-Json -Depth 16|Set-Content (Join-Path $output 'qualified-source.receipt.txt')
& (Join-Path $work 'Invoke-Qualification.ps1') -Root $Root -OutputDirectory $output
& (Join-Path $work 'Verify-FrozenInputs.ps1') -Root $Root -OutputDirectory $output
$native=Get-Content (Join-Path $output 'x64-Release/Embedded-x64-Release.json') -Raw|ConvertFrom-Json
$performance=Get-Content -LiteralPath $native.performanceReport -Raw|ConvertFrom-Json
& (Join-Path $work 'Capture-QualifiedArtifacts.ps1') -Root $Root -OutputDirectory (Join-Path $work 'FinalArtifacts') -ExpectedSource $performance.sourceFingerprint
& (Join-Path $work 'Invoke-ToolingQualification.ps1') -Root $Root -OutputDirectory $output
Write-Host 'Frozen native qualification, exact artifacts, complete tooling and pinned formatter finished; documentation/evidence closeout remains.'
