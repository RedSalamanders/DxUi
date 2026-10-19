[CmdletBinding()]
param([string] $Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$packetRoot=[IO.Path]::GetFullPath($Packet)
function Read-Receipt([string] $Relative) {
    Get-Content -LiteralPath (Join-Path $packetRoot $Relative) -Raw | ConvertFrom-Json
}
function Assert-Hash([string] $Relative,[string] $Expected) {
    $path=[IO.Path]::GetFullPath((Join-Path $packetRoot $Relative))
    if(!$path.StartsWith($packetRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Hash path escapes packet'}
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $Expected) {throw "Hash mismatch: $Relative"}
}
$files=Read-Receipt 'files.sha256.receipt.txt'
foreach($file in $files) {Assert-Hash $file.path $file.sha256}
$run=Read-Receipt 'Raw/runs.receipt.txt'
if($run.sourceCommit -cne 'a72ab562e5440cf4cb91917590c4729310d85ce3' -or !$run.completedUtc -or @($run.jobs).Count -ne 6) {throw 'Incomplete source-frozen matrix'}
$count=0
foreach($job in $run.jobs) {
    if($job.exitCode -ne 0 -or $job.sourceCommit -cne $run.sourceCommit) {throw 'Job outcome/source mismatch'}
    if(!$job.runtime) {continue}
    $configuration=switch($job.name) {'x64-Debug' {'Debug'} 'x64-Release' {'Release'} 'x64-ASanDebug' {'ASan Debug'} default {throw 'Unexpected runtime profile'}}
    $prefix='Raw/'+$job.name+'/'
    $artifacts=Read-Receipt ($prefix+'artifacts.sha256.receipt.txt')
    foreach($scope in @('EditorControls','Embedded','Accessibility')) {
        $receipt=Read-Receipt ($prefix+$scope+'-x64-'+$configuration+'.receipt.txt')
        if($receipt.exitCode -ne 0 -or @($receipt.skips).Count) {throw 'Scope result/skip mismatch'}
        $name=($receipt.executable -split '[\\/]')[-1]
        $binding=@($artifacts | Where-Object {($_.Path -split '[\\/]')[-1] -ceq $name})
        if($binding.Count -ne 1 -or $binding[0].Hash -cne $receipt.sha256) {throw 'Original scope/artifact hash binding mismatch'}
        ++$count
    }
    $native=Join-Path $packetRoot ($prefix+'test-EditorControls-x64-'+$configuration+'.log')
    foreach($name in @('TestRichSemanticDocumentNativeHtmlAndLinkPresentation','TestRichSemanticGeometryHangingMarkersLinksAndHtml','TestRichSemanticGeometryLinkRasterOracle','TestRichSemanticNativeHangingMarkersLinksAndHtml','TestRichSemanticGeometrySourceCaretSites','TestRichSemanticNativeSourceCaretSites')) {
        if(!(Select-String -LiteralPath $native -SimpleMatch ('[DONE] '+$name+' '))) {throw 'Directed native scenario completion missing'}
    }
    $client=Join-Path $packetRoot ($prefix+'test-Accessibility-x64-'+$configuration+'.log')
    foreach($name in @('TestRichTextAccessibilityWindowNativeHtmlAndMarkersClient','TestRichTextAccessibilityEmbeddedNativeHtmlAndMarkersClient','TestRichTextAccessibilityWindowSourceCaretSitesClient','TestRichTextAccessibilityEmbeddedSourceCaretSitesClient')) {
        if(!(Select-String -LiteralPath $client -SimpleMatch ('[DONE] '+$name+' '))) {throw 'Actual UIA client completion missing'}
    }
}
if($count -ne 9) {throw 'Nine ordinary scope receipts required'}
$audit=Read-Receipt 'Raw/runtime-artifact-audit.receipt.txt'
if($audit.sourceCommit -cne $run.sourceCommit -or $audit.archivedArtifactsVerified -ne 76 -or !$audit.asanDetectionArtifactMatches) {throw 'Original private artifact audit mismatch'}
$asan=Read-Receipt 'Raw/x64-ASanDebug/AddressSanitizer-x64.receipt.txt'
if(!$asan.detected) {throw 'Sanitizer detection result missing'}
$capture=Read-Receipt 'capture.receipt.txt'
if($capture.sourceCommit -cne $run.sourceCommit) {throw 'Capture source mismatch'}
Assert-Hash 'rich-text-preview.png' $capture.imageSha256
$gallery=Read-Receipt 'Raw/published-gallery-tile-audit.receipt.txt'
if(!$gallery.fiveReviewedComponentRegionsMatch -or @($gallery.images).Count -ne 5 -or @($gallery.images | Where-Object {!$_.equal}).Count) {throw 'Five reviewed gallery regions required'}
$comparison=Read-Receipt 'Raw/Diagnostic/candidate-Default.json.comparison.receipt.txt'
$exitRecord=Read-Receipt 'Raw/Diagnostic/candidate-performance.exit.receipt.txt'
if ($comparison.status -notin @('advice-required','within-noise-budget') -or ($comparison.status -ceq 'advice-required' -and $exitRecord.exitCode -ne 1) -or ($comparison.status -ceq 'within-noise-budget' -and $exitRecord.exitCode -ne 0)) {throw 'Diagnostic result/exit mismatch'}
Write-Output "Verified $(@($files).Count) packet-file hashes, nine scope bindings, original six-job audit and sample provenance. D4c remains HOLD."
Write-Output 'This replays retained evidence; it does not execute native suites or rehash private binaries absent from this packet.'
