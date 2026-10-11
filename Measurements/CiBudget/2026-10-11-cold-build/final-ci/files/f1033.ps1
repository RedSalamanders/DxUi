[CmdletBinding()]
param([Parameter(Mandatory)][string] $CollectionRoot, [Parameter(Mandatory)][string] $OutputPath)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [Text.UTF8Encoding]::new($false, $true)
function Need([bool] $Value, [string] $Message) { if (-not $Value) { throw $Message } }
function Hash([string] $Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Read([string] $Path) { $utf8.GetString([IO.File]::ReadAllBytes($Path)) | ConvertFrom-Json -AsHashtable }
$CollectionRoot = [IO.Path]::GetFullPath($CollectionRoot)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
Need (-not (Test-Path -LiteralPath $OutputPath)) 'Derived output already exists.'
Need ($OutputPath.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) 'Output must be a new derived sibling within the collection, outside its original seal.'
$sealPath = Join-Path $CollectionRoot 'SHA256SUMS'
$sealHash = Hash $sealPath
function Check-Seal {
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in [IO.File]::ReadAllLines($sealPath)) {
        Need ($line -cmatch '^(?<sha>[a-f0-9]{64})  (?<relative>.+)$') 'Malformed original seal.'
        $sha = $Matches.sha; $relative = $Matches.relative
        $path = [IO.Path]::GetFullPath((Join-Path $CollectionRoot $relative))
        Need ($path.StartsWith($CollectionRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and $seen.Add($path)) 'Escaped or duplicate seal path.'
        Need ((Hash $path).ToLowerInvariant() -ceq $sha) "Original evidence changed: $relative"
    }
    Need ($seen.Count -eq 1021) 'Original full capture inventory differs.'
}
Check-Seal
$fullPath = Join-Path $CollectionRoot 'review-20261011T214113Z-9736815beaff470294e019f3f1a05bd8.receipt.txt'
$external = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$functionalPath = Join-Path $external 'early-ci-native-cold-budget/root-functional-visual-399-318fd6cdb9ac4db68bcf4a942c7321f0.receipt.txt'
$functionalDriver = Join-Path $external 'early-ci-native-cold-budget/Review-FunctionalVisual-399.ps1'
Need ((Hash $functionalDriver) -ceq '08C6496F5D6A9B39EC1B0492682D8A33AF1A09963D19E5DEFBF81B6C0B46DDC3') 'Previously root-reviewed functional driver changed.'
$sourceRoot = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
function Check-Source {
    $head = (& git -C $sourceRoot rev-parse HEAD).Trim()
    Need ($LASTEXITCODE -eq 0 -and $head -ceq '4bd3480170140090ab2d8ff883d05507281b52ee') 'Frozen source HEAD differs.'
    $tree = (& git -C $sourceRoot rev-parse 'HEAD^{tree}').Trim()
    Need ($LASTEXITCODE -eq 0 -and $tree -ceq 'e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d') 'Frozen source tree differs.'
    $status = @(& git -C $sourceRoot status --porcelain --untracked-files=normal)
    Need ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Frozen source is dirty.'
}
Check-Source
$inputs = [ordered]@{}
foreach ($path in @($fullPath, $functionalPath, $functionalDriver)) { $inputs[$path] = Hash $path }
$full = Read $fullPath; $functional = Read $functionalPath
foreach ($r in @($full, $functional)) {
    Need ($r.runId -eq 38172114803 -and $r.attempt -eq 1 -and $r.head -ceq '4bd3480170140090ab2d8ff883d05507281b52ee' -and $r.testedMerge -ceq '80d875a1de91d2b2c8602faa5614cf75eab6025b') 'Current source/run binding differs.'
}
Need ($full.status -ceq 'BLOCKED') 'Original full-review failure must remain explicit.'
$bound = [Collections.Generic.List[object]]::new()
$gallery = [Collections.Generic.List[object]]::new()
$seenProfiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($profile in $functional.profiles) {
    Need ($seenProfiles.Add($profile.profile) -and @($profile.tests).Count -eq 38) 'Incomplete or duplicate root functional profile.'
    $partialPath = [string]$profile.review
    Need ((Hash $partialPath) -ceq $profile.reviewSha256) 'Previously reviewed native packet changed.'
    $inputs[$partialPath] = Hash $partialPath
    $partial = Read $partialPath
    Need ($partial.head -ceq $full.head -and $partial.testedMerge -ceq $full.testedMerge -and @($partial.blockers).Count -eq 0) 'Partial review source/blockers differ.'
    $fullNative = @($full.native | Where-Object { $_.profile -ceq $profile.profile })
    Need ($fullNative.Count -eq @($partial.native).Count) 'Current native inventory differs between partial and final capture.'
    foreach ($n in $partial.native) {
        $matches = @($fullNative | Where-Object { $_.suite -ceq $n.suite })
        Need ($matches.Count -eq 1) 'Missing or duplicate full native suite.'
        $match = $matches[0]
        foreach ($key in @('exitCode','receiptSha256','suiteLogSha256','executableSha256','buildInputIdentity','buildArtifactIdentity','attestation','possibleDatedArmRefusal')) {
            Need ($match[$key] -ceq $n[$key]) "Full capture differs from reviewed current-source suite: $($n.profile)/$($n.suite)/$key"
        }
        foreach ($pair in @(@('receipt','receiptSha256'), @('suiteLog','suiteLogSha256'))) {
            Need ((Hash $match[$pair[0]]) -ceq $match[$pair[1]] -and (Hash $n[$pair[0]]) -ceq $n[$pair[1]]) 'Actual full/partial native bytes differ from their shared digest.'
        }
        Need ((Hash $match.performance) -ceq (Hash $n.performance)) 'Current raw performance bytes differ between partial and final capture.'
        $bound.Add([ordered]@{ profile=$n.profile; suite=$n.suite; exitCode=$n.exitCode; finalReceipt=$match.receipt; finalReceiptSha256=$match.receiptSha256; finalLog=$match.suiteLog; finalLogSha256=$match.suiteLogSha256; partialReview=$partialPath })
    }
    foreach ($image in $profile.gallery) {
        $name = [IO.Path]::GetFileName([string]$image.file)
        $matches = @($full.freshGalleryImagesRequiringHumanVisualReview | Where-Object { $_.profile -ceq $profile.profile -and [IO.Path]::GetFileName([string]$_.file) -ceq $name })
        Need ($matches.Count -eq 1) 'Missing or duplicate full harness gallery image.'
        $match = $matches[0]
        Need ($match.sha256 -ceq $image.sha256 -and (Hash $match.file) -ceq $image.sha256 -and (Hash $image.file) -ceq $image.sha256) 'Current full image differs from root-viewed harness image bytes.'
        Need ($match.sourceCommit -ceq $full.testedMerge) 'Actual harness image source differs.'
        $gallery.Add([ordered]@{ profile=$profile.profile; finalImage=$match.file; sha256=$image.sha256; originalRootViewedImage=$image.file; reviewedVisual=$image.visual; sameHistoricalPixels=$image.sameHistoricalPixels; freshVisualReceipt=$image.freshVisualReceipt })
    }
}
Need ($seenProfiles.Count -eq 6 -and $bound.Count -eq 128 -and $gallery.Count -eq 18) 'Incomplete final functional/visual binding.'
foreach ($path in $inputs.Keys) { Need ((Hash $path) -ceq $inputs[$path]) 'A reviewed input changed during binding.' }
Check-Source
Check-Seal
Need ((Hash $sealPath) -ceq $sealHash) 'Original seal changed.'
$receipt = [ordered]@{
    status='ROOT_FULL_CAPTURE_FUNCTIONAL_AND_VISUAL_BYTES_BOUND'; ciGateSucceeded=$false; qualificationPassed=$false;
    runId=38172114803; attempt=1; head=$full.head; testedMerge=$full.testedMerge; testedTree=$full.testedTree;
    originalFullReviewStatus=$full.status; originalSealedFiles=1021; originalSealSha256=$sealHash; originalSealUnchanged=$true;
    inputs=$inputs; driver=$PSCommandPath; driverSha256=(Hash $PSCommandPath);
    currentSourceFunctionsPerProfile=38; profiles=6; functionExecutions=228; nativeReceipts=$bound.ToArray(); harnessGalleryImages=$gallery.ToArray();
    originalAssemblyAttempt='The earlier ad hoc binding selected nonexistent suite Controls. The actual suite is Control; both queried records were null. This driver first used the wrong external location for the previously reviewed driver; rg located it and the location was corrected before binding. No measurement or source failed, and no successful receipt was emitted by either setup attempt.';
    scope='Exact same current-run receipt/log/performance/image bytes as the root-reviewed partial native packets. This binds the retained final collection to those functional and visual reviews. All six unlaunched ARM foreground suites and the original paired/aggregate failures remain failed/incomplete.';
    completedUtc=[DateTimeOffset]::UtcNow
}
$bytes=$utf8.GetBytes(($receipt | ConvertTo-Json -Depth 20) + [char]10)
$stream=[IO.File]::Open($OutputPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
[ordered]@{ output=$OutputPath; sha256=(Hash $OutputPath); status=$receipt.status; profiles=6; functions=228; nativeReceipts=128; gallery=18; ciGateSucceeded=$false } | ConvertTo-Json
