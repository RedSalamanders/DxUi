[CmdletBinding()]
param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$packetRoot=[IO.Path]::GetFullPath($Packet)
$expected='5bc432845cf29683b2121101130085f153d8bda7'
function Read-Receipt([string]$Relative) {Get-Content -LiteralPath (Join-Path $packetRoot $Relative) -Raw | ConvertFrom-Json}
$files=Read-Receipt 'files.sha256.receipt.txt'
foreach($file in $files) {
    $path=[IO.Path]::GetFullPath((Join-Path $packetRoot $file.path))
    if(!$path.StartsWith($packetRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
       (Get-FileHash -LiteralPath $path).Hash -cne $file.sha256){throw 'Packet path/hash mismatch'}
}
$run=Read-Receipt 'Raw/runs.receipt.txt'
if($run.sourceCommit -cne $expected -or !$run.completedUtc -or @($run.jobs).Count -ne 6 -or @($run.scopes).Count -ne 19){throw 'Incomplete frozen matrix'}
$scopeCount=0;$artifactCount=0
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($job in $run.jobs) {
    if($job.sourceCommit -cne $expected -or $job.exitCode -ne 0 -or !$seen.Add($job.name)){throw 'Profile source/outcome mismatch'}
    $prefix='Raw/'+$job.name+'/'
    $artifacts=Read-Receipt ($prefix+'artifacts.sha256.receipt.txt')
    $artifactCount+=@($artifacts).Count
    if(!$job.runtime){continue}
    if($job.platform -cne 'x64' -or $job.configuration -notin @('Debug','Release','ASan Debug')){throw 'Unexpected runtime architecture/profile'}
    foreach($scope in $run.scopes) {
        $report=Read-Receipt ($prefix+$scope+'-x64-'+$job.configuration+'.receipt.txt')
        $name=($report.executable -split '[\\/]')[-1]
        $binding=@($artifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq $name})
        if($report.suite -cne $scope -or $report.configuration -cne $job.configuration -or $report.platform -cne 'x64' -or $report.nativeArchitecture -cne 'X64' -or
           $report.exitCode -ne 0 -or @($report.skips).Count -or $binding.Count -ne 1 -or $binding[0].Hash -cne $report.sha256){throw 'Scope/executable binding mismatch'}
        ++$scopeCount
    }
    $native=Join-Path $packetRoot ($prefix+'test-EditorControls-x64-'+$job.configuration+'.log')
    foreach($name in @('TestRichSemanticGeometryTaskCheckboxAlignment','TestRichSemanticNativeTaskCheckboxAlignment','TestRichSemanticGeometryLinkRasterOracle')) {
        if(!(Select-String -LiteralPath $native -SimpleMatch ('[DONE] '+$name+' '))){throw 'New native/raster test completion missing'}
    }
}
$audit=Read-Receipt 'Raw/runtime-artifact-audit.receipt.txt'
if($scopeCount -ne 57 -or $audit.sourceCommit -cne $expected -or $audit.ordinaryScopeReceipts -ne 57 -or $audit.archivedArtifactsVerified -ne $artifactCount -or
   !$audit.zeroCapabilitySkips -or !$audit.asanDetectionArtifactMatches){throw 'Archive audit mismatch'}
$fresh=Read-Receipt 'Raw/FreshARM64/runs.receipt.txt'
if($fresh.sourceCommit -cne $expected -or !$fresh.completedUtc -or @($fresh.jobs).Count -ne 2 -or $audit.freshArm64ArtifactBindings -ne 18){throw 'Fresh ARM64 compiler records incomplete'}
foreach($job in $fresh.jobs) {
    if($job.sourceCommit -cne $expected -or $job.exitCode -ne 0){throw 'Fresh ARM64 outcome mismatch'}
    $original=Read-Receipt ('Raw/FreshARM64/'+$job.configuration+'/artifacts.sha256.receipt.txt')
    $finalArtifacts=Read-Receipt ('Raw/ARM64-'+$job.configuration+'/artifacts.sha256.receipt.txt')
    foreach($file in $finalArtifacts | Where-Object {[IO.Path]::GetExtension($_.Path) -in @('.exe','.pdb','.lib','.dll')}) {
        $name=($file.Path -split '[\\/]')[-1]
        $binding=@($original | Where-Object {($_.Path -split '[\\/]')[-1] -ceq $name})
        if($binding.Count -ne 1 -or $binding[0].Hash -cne $file.Hash){throw 'Fresh/final ARM64 artifact mismatch'}
    }
}
$asan=Read-Receipt 'Raw/x64-ASanDebug/AddressSanitizer-x64.receipt.txt'
if(!$asan.detected){throw 'Intentional ASan detection missing'}
$capture=Read-Receipt 'capture.receipt.txt'
$release=Read-Receipt 'Raw/x64-Release/artifacts.sha256.receipt.txt'
$sample=@($release | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.EmbeddedControls.exe'})
if($capture.sourceCommit -cne $expected -or $capture.sourceDirty -or $sample.Count -ne 1 -or $sample[0].Hash -cne $capture.executableSha256 -or
   (Get-FileHash -LiteralPath (Join-Path $packetRoot 'rich-text-tasks.png')).Hash -cne $capture.imageSha256){throw 'Sample provenance mismatch'}
$crop=Read-Receipt 'crop.receipt.txt'
if($crop.sourceSha256 -cne $capture.imageSha256 -or $crop.rescaled -or
   (Get-FileHash -LiteralPath (Join-Path $packetRoot 'task-alignment-detail.png')).Hash -cne $crop.cropSha256){throw 'Derived sample crop provenance mismatch'}
$gallery=Read-Receipt 'Raw/gallery-review.receipt.txt'
$generation=Read-Receipt 'Raw/generation.receipt.txt'
$control=@($release | Where-Object {($_.Path -split '[\\/]')[-1] -ceq 'DxUi.ControlTests.exe'})
if($gallery.sourceCommit -cne $expected -or $gallery.sourceDirty -or $gallery.fullSheetsReviewed -ne 5 -or !$gallery.sampleReviewed -or
   !$gallery.nativeScaleTaskRegionsReviewed -or $generation.sourceCommit -cne $expected -or $generation.sourceDirty -or
   $control.Count -ne 1 -or $gallery.controlExecutableSha256 -cne $control[0].Hash -or
   $gallery.sampleExecutableSha256 -cne $capture.executableSha256 -or $gallery.sampleImageSha256 -cne $capture.imageSha256){throw 'Gallery review/source/artifact mismatch'}
foreach($entry in $generation.images) {
    $reviewed=@($gallery.images | Where-Object {$_.file -ceq $entry.file})
    if($reviewed.Count -ne 1 -or $reviewed[0].sha256 -cne $entry.sha256){throw 'Published image differs from reviewed image'}
}
$final=Read-Receipt 'Raw/final-native-equivalence.receipt.txt'
if($final.qualifiedSourceCommit -cne $expected -or $final.nativeInputsChanged -or !$final.metadataOnly -or $final.currentNativeArtifactsRehashed -ne 25 -or
   @($final.unexpectedTrackedPaths).Count -or @($final.unexpectedUntrackedPaths).Count -or @($final.artifacts | Where-Object {!$_.archiveMatches}).Count){throw 'Metadata-only native equivalence mismatch'}
$validators=Read-Receipt 'Raw/final-validators.receipt.txt'
if(@($validators).Count -ne 5 -or @($validators | Where-Object {$_.exitCode -ne 0}).Count){throw 'Final metadata validation incomplete'}
Write-Output "Verified $(@($files).Count) packet-file hashes, 57 scope/executable bindings, six profiles, sanitizer detection and reviewed captures."
Write-Output 'This replays retained evidence, not native execution or absent private binary hashes. D4c remains HOLD.'
