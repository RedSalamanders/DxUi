[CmdletBinding()]
param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$packetRoot=[IO.Path]::GetFullPath($Packet)
$expected='5717ff85a7e0aecb5374cd2ff7d276f368d9e578'
function Packet-Path([string]$Relative) {
    if([IO.Path]::IsPathRooted($Relative)){throw 'Absolute packet path'}
    $path=[IO.Path]::GetFullPath((Join-Path $packetRoot $Relative))
    if(!$path.StartsWith($packetRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Path escapes packet'}
    return $path
}
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Packet-Path $Relative) -Raw | ConvertFrom-Json}
function Leaf([string]$Path){($Path -split '[\\/]')[-1]}
function Binding($Artifacts,[string]$Name) {
    $found=@($Artifacts | Where-Object {(Leaf $_.Path) -ceq $Name})
    if($found.Count -ne 1){throw "Missing/duplicate artifact $Name"}
    return $found[0].Hash
}
$files=Read-Receipt 'files.sha256.receipt.txt'
$seenFiles=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in $files) {
    if(!$seenFiles.Add($file.path) -or (Get-FileHash -LiteralPath (Packet-Path $file.path)).Hash -cne $file.sha256){throw 'Packet path/hash mismatch'}
}
$run=Read-Receipt 'Raw/runs.receipt.txt'
$profiles=@('x64-Debug','x64-Release','x64-ASanDebug','ARM64-Debug','ARM64-Release','ARM64-ASanDebug')
if($run.sourceCommit -cne $expected -or !$run.completedUtc -or @($run.jobs).Count -ne 6 -or @($run.scopes).Count -ne 19 -or @($run.scopes | Sort-Object -Unique).Count -ne 19){throw 'Incomplete frozen matrix'}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$scopeCount=0;$artifactCount=0
foreach($job in $run.jobs) {
    if($job.name -cnotin $profiles -or !$seen.Add($job.name) -or $job.sourceCommit -cne $expected -or $job.exitCode -ne 0){throw 'Unexpected profile/outcome/source'}
    $prefix='Raw/'+$job.name+'/'
    $artifacts=Read-Receipt ($prefix+'artifacts.sha256.receipt.txt')
    $artifactCount+=@($artifacts).Count
    # Raw manifests retain original private paths. Rehash only public payloads here;
    # absent native binaries are bound by their retained hashes, never re-executed.
    foreach($entry in $artifacts | Where-Object {[IO.Path]::GetExtension($_.Path) -in @('.json','.log')}) {
        $name=Leaf $entry.Path
        if($name -like 'Performance-*.json.comparison.json'){$name='Default.comparison.json'}
        elseif($name -like 'Performance-*.json'){$name='Default.json'}
        if($name.EndsWith('.json')){$name=$name.Substring(0,$name.Length-5)+'.receipt.txt'}
        if((Get-FileHash -LiteralPath (Packet-Path ($prefix+$name))).Hash -cne $entry.Hash){throw 'Raw archive payload changed'}
    }
    $state=Read-Receipt ($prefix+'verified-build-state.receipt.txt')
    $attestation=Read-Receipt ($prefix+'build-attestation.receipt.txt')
    $incremental=Read-Receipt ($prefix+'incremental-attestation.receipt.txt')
    $identityText=@('dxui-performance-build-v2',2,$state.resolvedBuildIdentity,$state.buildInputIdentity,$state.toolchainIdentity,$state.buildArtifactIdentity) -join "`n"
    $identity=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($identityText))).ToLowerInvariant()
    if($state.sourceCommit -cne $expected -or $state.sourceDirty -or $state.buildIdentityVersion -ne 2 -or $state.identityStatus -cne 'verifiable' -or
       $state.platform -cne $job.platform -or $state.configuration -cne $job.configuration -or
       $state.executableSha256 -cne (Binding $artifacts 'DxUi.EmbeddedTests.exe') -or
       $state.buildAttestationIdentity -cne $identity -or $attestation.identity -cne $identity -or
       $incremental.version -ne 1 -or $incremental.identity -cnotmatch '^[a-f0-9]{64}$' -or
       $attestation.outcome -cne 'PASSED' -or $incremental.outcome -cne 'PASSED'){throw 'v2 build/source/artifact attestation mismatch'}
    # Incremental receipts bind a separate private intermediate inventory. Retain
    # their exact bytes; this offline packet does not reconstruct that inventory.
    if(!$job.runtime){if($job.platform -cne 'ARM64'){throw 'Unexpected compile-only job'};continue}
    if($job.platform -cne 'x64' -or $job.configuration -cnotin @('Debug','Release','ASan Debug')){throw 'Unexpected runtime profile'}
    foreach($scope in $run.scopes) {
        $report=Read-Receipt ($prefix+$scope+'-x64-'+$job.configuration+'.receipt.txt')
        if($report.suite -cne $scope -or $report.configuration -cne $job.configuration -or $report.platform -cne 'x64' -or $report.nativeArchitecture -cne 'X64' -or
           $report.exitCode -ne 0 -or @($report.skips).Count -or $report.sha256 -cne (Binding $artifacts (Leaf $report.executable))){throw 'Scope/executable binding mismatch'}
        ++$scopeCount
    }
    $editor=Packet-Path ($prefix+'test-EditorControls-x64-'+$job.configuration+'.log')
    foreach($name in @('TestRichSemanticDocumentSourceFormatOwnership','TestRichSemanticDocumentPinnedSourceCases')) {
        if(!(Select-String -LiteralPath $editor -SimpleMatch ('[DONE] '+$name+' '))){throw 'Source-format test completion missing'}
    }
    if(!(Select-String -LiteralPath $editor -SimpleMatch 'semantic pinned source cases admitted=1324 rejectedDocumentSource=0 failures=0')){throw 'Pinned source admissions incomplete'}
    $accessibility=Packet-Path ($prefix+'test-Accessibility-x64-'+$job.configuration+'.log')
    foreach($hostKind in @('Window','Embedded')) {
        foreach($behavior in @('SemanticAttributesAndFormats','SemanticStructureAndTasks','SemanticCompositionBindingAndRetirement')) {
            if(!(Select-String -LiteralPath $accessibility -SimpleMatch ('[DONE] TestRichTextAccessibility'+$hostKind+$behavior+' '))){throw 'Semantic UIA completion missing'}
        }
    }
    $embedded=Packet-Path ($prefix+'test-Embedded-x64-'+$job.configuration+'.log')
    if(!(Select-String -LiteralPath $embedded -Pattern 'rich semantic native dispatch sweptRequests=[1-9][0-9]* sourcePreserved=1 retry=1 focusUnchanged=1') -or
       !(Select-String -LiteralPath $embedded -Pattern 'PASS [0-9]+ checks; 1000 warm composites [0-9]+ us; C\+\+ allocations 0;')){throw 'Failure/resource test completion missing'}
}
$audit=Read-Receipt 'Raw/runtime-artifact-audit.receipt.txt'
if($scopeCount -ne 57 -or $audit.sourceCommit -cne $expected -or $audit.archivedArtifactsVerified -ne $artifactCount -or $audit.ordinaryScopeReceipts -ne 57 -or !$audit.zeroCapabilitySkips -or !$audit.asanDetectionArtifactMatches){throw 'Archive audit mismatch'}
$fresh=Read-Receipt 'Raw/FreshARM64/runs.receipt.txt'
if($fresh.sourceCommit -cne $expected -or !$fresh.completedUtc -or @($fresh.jobs).Count -ne 2){throw 'Fresh ARM64 records incomplete'}
$freshBindings=0
foreach($job in $fresh.jobs) {
    if($job.sourceCommit -cne $expected -or $job.exitCode -ne 0){throw 'Fresh ARM64 outcome mismatch'}
    $original=Read-Receipt ('Raw/FreshARM64/'+$job.configuration+'/artifacts.sha256.receipt.txt')
    $final=Read-Receipt ('Raw/ARM64-'+$job.configuration+'/artifacts.sha256.receipt.txt')
    foreach($entry in $final | Where-Object {[IO.Path]::GetExtension($_.Path) -in @('.exe','.pdb','.lib','.dll')}) {
        if($entry.Hash -cne (Binding $original (Leaf $entry.Path))){throw 'Fresh/final ARM64 mismatch'}
        ++$freshBindings
    }
}
if($freshBindings -ne 18 -or $audit.freshArm64ArtifactBindings -ne $freshBindings){throw 'Fresh ARM64 binding count mismatch'}
$asan=Read-Receipt 'Raw/x64-ASanDebug/AddressSanitizer-x64.receipt.txt'
$asanArtifacts=Read-Receipt 'Raw/x64-ASanDebug/artifacts.sha256.receipt.txt'
if(!$asan.detected -or $asan.sha256 -cne (Binding $asanArtifacts 'DxUi.FoundationTests.exe')){throw 'Sanitizer detection binding mismatch'}
$capture=Read-Receipt 'Raw/capture.receipt.txt'
$review=Read-Receipt 'Raw/gallery-review.receipt.txt'
$release=Read-Receipt 'Raw/x64-Release/artifacts.sha256.receipt.txt'
if($capture.sourceCommit -cne $expected -or $capture.sourceDirty -or $review.sourceCommit -cne $expected -or $review.sourceDirty -or
   $review.fullSheetsReviewed -ne 5 -or !$review.nativeScaleTaskRegionsReviewed -or !$review.genericEmbeddedReviewed -or !$review.richSampleReviewed -or
   $capture.sampleSha256 -cne (Binding $release 'DxUi.EmbeddedControls.exe') -or $capture.controlSha256 -cne (Binding $release 'DxUi.ControlTests.exe') -or
   $review.sampleImageSha256 -cne $capture.richTextSha256 -or $capture.richTextSha256 -cne (Get-FileHash -LiteralPath (Packet-Path 'rich-text.png')).Hash){throw 'Capture/review binding mismatch'}
$regions=Read-Receipt 'Raw/current-gallery-region-comparison.receipt.txt'
if(@($regions).Count -ne 5){throw 'Incomplete themed region review'}
foreach($region in $regions) {
    if(!$region.equal -or $region.candidateRegionSha256 -cne $region.baselineRegionSha256 -or
       $region.candidateRegionSha256 -cne (Get-FileHash -LiteralPath (Packet-Path ('RichTextTiles/'+$region.file))).Hash){throw 'Rich visual region changed'}
}
$negative=Read-Receipt 'Raw/NegativeDiagnostic/result.receipt.txt'
$diagnostic=Read-Receipt 'Raw/Diagnostic/candidate.exit.receipt.txt'
if($negative.exitCode -ne 3 -or $diagnostic.sourceCommit -cne $expected -or $diagnostic.exitCode -ne 1){throw 'Preserved failing diagnostic outcome changed'}
$validation=Get-Content -LiteralPath (Packet-Path 'Raw/iteration8-validate.log') -Raw
if(!$validation.Contains('All validation passed.') -or !$validation.Contains('All tooling tests passed.')){throw 'Qualification validation missing'}
$final=Read-Receipt 'Raw/final-native-equivalence.receipt.txt'
if($final.qualifiedSourceCommit -cne $expected -or $final.nativeInputsChanged -or !$final.metadataOnly -or $final.currentNativeArtifactsRehashed -ne 25 -or
   @($final.unexpectedTrackedPaths).Count -or @($final.unexpectedUntrackedPaths).Count -or @($final.artifacts | Where-Object {!$_.archiveMatches}).Count){throw 'Metadata-only native equivalence mismatch'}
if(!(Get-Content -LiteralPath (Packet-Path 'Raw/final-validate.log') -Raw).Contains('All validation passed.')){throw 'Final metadata validation missing'}
Write-Output "Verified $(@($files).Count) packet-file hashes, 57 scope/executable bindings, six v2 build states, sanitizer detection and reviewed captures."
Write-Output 'Evidence replay only; absent private binaries are not rehashed or executed. D4c remains HOLD; ARM64 is compile-only.'
