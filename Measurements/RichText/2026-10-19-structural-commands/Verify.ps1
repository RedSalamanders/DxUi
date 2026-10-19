[CmdletBinding()]
param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$packetRoot=[IO.Path]::GetFullPath($Packet)
$expected='159b77c7700e5e1746bd9846265938cd6d0dbe0b'
function Read-Receipt([string]$Relative) {Get-Content -LiteralPath (Join-Path $packetRoot $Relative) -Raw | ConvertFrom-Json}
function Assert-Hash([string]$Relative,[string]$ExpectedHash) {
    $path=[IO.Path]::GetFullPath((Join-Path $packetRoot $Relative))
    if(!$path.StartsWith($packetRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Hash path escapes packet'}
    if((Get-FileHash -LiteralPath $path).Hash -cne $ExpectedHash){throw "Packet hash mismatch: $Relative"}
}
$files=Read-Receipt 'files.sha256.receipt.txt'
foreach($file in $files){Assert-Hash $file.path $file.sha256}
$run=Read-Receipt 'Raw/runs.receipt.txt'
if($run.sourceCommit -cne $expected -or !$run.completedUtc -or @($run.jobs).Count -ne 6){throw 'Incomplete frozen six-job matrix'}
$componentScopes=@('EditorControls','Embedded','Accessibility','TextField','MultilineText','WindowHost')
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$privateCount=0;$count=0
function Check-Scope([string]$Prefix,[string]$Scope,[string]$Configuration,[string]$Profile,$Artifacts) {
    $report=Read-Receipt ($Prefix+$Scope+'-x64-'+$Configuration+'.receipt.txt')
    if($report.suite -cne $Scope -or $report.configuration -cne $Configuration -or $report.platform -cne 'x64' -or $report.nativeArchitecture -cne 'X64' -or $report.exitCode -ne 0 -or @($report.skips).Count){throw 'Scope outcome/profile mismatch'}
    $name=($report.executable -split '[\\/]')[-1]
    $binding=@($Artifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq $name})
    if($binding.Count -ne 1 -or $binding[0].Hash -cne $report.sha256){throw 'Scope/private-artifact binding mismatch'}
    if(!$seen.Add($Profile+'/'+$Scope)){throw 'Duplicate ordinary scope/profile'}
}
foreach($job in $run.jobs) {
    if($job.sourceCommit -cne $expected -or $job.exitCode -ne 0){throw 'Main job source/outcome mismatch'}
    $prefix='Raw/'+$job.name+'/'
    $artifacts=Read-Receipt ($prefix+'artifacts.sha256.receipt.txt');$privateCount+=@($artifacts).Count
    if(!$job.runtime){continue}
    $configuration=switch($job.name){'x64-Debug'{'Debug'}'x64-Release'{'Release'}'x64-ASanDebug'{'ASan Debug'}default{throw 'Unexpected native profile'}}
    foreach($scope in $componentScopes){Check-Scope $prefix $scope $configuration $job.name $artifacts;++$count}
    $native=Join-Path $packetRoot ($prefix+'test-EditorControls-x64-'+$configuration+'.log')
    foreach($name in @('TestRichMarkdownStructuralListsAndHeadings','TestRichMarkdownStructuralInlineTasksLinks','TestRichMarkdownStructuralTables','TestRichMarkdownStructuralGuardsAndDeletion','TestRichMarkdownNativeStructuralCommands','TestRichMarkdownNativeHostStructuralRouting','TestRichSemanticDocumentStructuralStyleValues','TestRichSemanticDocumentPinnedSourceCases')) {
        if(!(Select-String -LiteralPath $native -SimpleMatch ('[DONE] '+$name+' '))){throw 'Directed native completion missing'}
    }
    $client=Join-Path $packetRoot ($prefix+'test-Accessibility-x64-'+$configuration+'.log')
    foreach($name in @('TestRichTextAccessibilityWindowSemanticRenderingClient','TestRichTextAccessibilityEmbeddedSemanticRenderingClient')) {
        if(!(Select-String -LiteralPath $client -SimpleMatch ('[DONE] '+$name+' '))){throw 'Actual UIA client completion missing'}
    }
}
if($count -ne 18){throw 'Eighteen component scope receipts required'}
$audit=Read-Receipt 'Raw/runtime-artifact-audit.receipt.txt'
if($audit.sourceCommit -cne $expected -or $audit.archivedArtifactsVerified -ne $privateCount -or !$audit.asanDetectionArtifactMatches){throw 'Original private artifact audit mismatch'}
$extra=Read-Receipt 'Raw/Additional/runs.receipt.txt'
if($extra.sourceCommit -cne $expected -or !$extra.completedUtc -or @($extra.jobs).Count -ne 3 -or @($extra.scopes).Count -ne 13){throw 'Additional coverage incomplete'}
foreach($job in $extra.jobs) {
    if($job.sourceCommit -cne $expected -or $job.exitCode -ne 0){throw 'Additional outcome mismatch'}
    $prefix='Raw/Additional/'+$job.name+'/'
    $artifacts=Read-Receipt ($prefix+'artifacts.sha256.receipt.txt')
    $configuration=switch($job.name){'x64-Debug'{'Debug'}'x64-Release'{'Release'}'x64-ASanDebug'{'ASan Debug'}default{throw 'Unexpected additional profile'}}
    foreach($scope in $extra.scopes){Check-Scope $prefix $scope $configuration $job.name $artifacts;++$count}
}
if($count -ne 57){throw '57 distinct ordinary scope/profile receipts required'}
$all=Read-Receipt 'Raw/all-runtime-artifact-audit.receipt.txt'
if($all.sourceCommit -cne $expected -or $all.totalOrdinaryScopeReceipts -ne 57 -or !$all.sameExecutableHashesAsComponentMatrix -or !$all.zeroCapabilitySkips){throw 'Aggregate ordinary qualification mismatch'}
$final=Read-Receipt 'Raw/final-native-equivalence.receipt.txt'
if($final.qualifiedSourceCommit -cne $expected -or $final.nativeInputsChanged -or !$final.metadataOnly -or $final.currentNativeArtifactsRehashed -ne 25 -or
   @($final.unexpectedTrackedPaths).Count -or @($final.unexpectedUntrackedPaths).Count -or @($final.artifacts | Where-Object {!$_.archiveMatches}).Count){throw 'Final native equivalence audit mismatch'}
$validators=Read-Receipt 'Raw/final-validators.receipt.txt'
if(@($validators).Count -ne 5 -or @($validators | Where-Object {$_.exitCode -ne 0}).Count){throw 'Final metadata validation incomplete'}
$asan=Read-Receipt 'Raw/x64-ASanDebug/AddressSanitizer-x64.receipt.txt'
if(!$asan.detected){throw 'ASan detection missing'}
$capture=Read-Receipt 'capture.receipt.txt'
if($capture.sourceCommit -cne $expected -or $capture.sourceDirty){throw 'Sample provenance mismatch'}
Assert-Hash 'rich-text-commands.png' $capture.imageSha256
$sampleArtifacts=Read-Receipt 'sample-artifacts.sha256.receipt.txt'
$sampleBinary=@($sampleArtifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.EmbeddedControls.exe'})
$sampleLibrary=@($sampleArtifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.lib'})
$releaseArtifacts=Read-Receipt 'Raw/x64-Release/artifacts.sha256.receipt.txt'
$releaseLibrary=@($releaseArtifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.lib'})
$releaseControls=@($releaseArtifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.ControlTests.exe'})
$review=Read-Receipt 'Raw/gallery-review.receipt.txt'
if($sampleBinary.Count -ne 1 -or $sampleLibrary.Count -ne 1 -or $releaseLibrary.Count -ne 1 -or $releaseControls.Count -ne 1 -or
   $sampleBinary[0].Hash -cne $capture.executableSha256 -or $sampleLibrary[0].Hash -cne $releaseLibrary[0].Hash -or
   $review.sourceCommit -cne $expected -or $review.sourceDirty -or $review.fullSheetsReviewed -ne 5 -or !$review.sampleReviewed -or
   $review.sampleImageSha256 -cne $capture.imageSha256 -or $review.embeddedExecutableSha256 -cne $capture.executableSha256 -or
   $review.controlExecutableSha256 -cne $releaseControls[0].Hash){throw 'Reviewed capture/artifact binding mismatch'}
$gallery=Read-Receipt 'Raw/published-gallery-tile-audit.receipt.txt'
if($gallery.sourceCommit -cne $expected -or !$gallery.fiveReviewedComponentRegionsMatch -or @($gallery.images).Count -ne 5 -or @($gallery.images | Where-Object {!$_.equal}).Count){throw 'Five reviewed/published gallery regions required'}
$comparison=Read-Receipt 'Raw/Diagnostic/candidate-Default.json.comparison.receipt.txt'
$exitRecord=Read-Receipt 'Raw/Diagnostic/candidate-performance.exit.receipt.txt'
if($comparison.status -notin @('advice-required','within-noise-budget') -or ($comparison.status -ceq 'advice-required' -and $exitRecord.exitCode -ne 1) -or ($comparison.status -ceq 'within-noise-budget' -and $exitRecord.exitCode -ne 0)){throw 'Diagnostic result/exit mismatch'}
Write-Output "Verified $(@($files).Count) packet-file hashes, 57 distinct scope bindings, six-job audit and sample provenance. D4c remains HOLD."
Write-Output 'This replays retained evidence; it does not execute native suites or rehash absent private binaries.'
