<# Root read-only review of new test completions and retained harness pixels.
Native/provenance/consumer validation remains with the separately frozen profile reviewer.
No build, native execution, repository mutation or relabeling of historical test qualification. #>
[CmdletBinding()] param([Parameter(Mandatory)][string[]] $ReviewReceipt)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRepo = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$taskHead = '4bd3480170140090ab2d8ff883d05507281b52ee'
$taskMerge = '80d875a1de91d2b2c8602faa5614cf75eab6025b'
$taskHistorical = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/early-ci-native-398/root-x64-functional-visual.receipt.txt'
$taskHistoricalSha = 'F58801F00795B290232B7B5A6CD086768A51E12A5261A263CB2E98AE292024C3'
$taskFreshGallery = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge/early-ci-native-cold-budget/root-viewed-asan-gallery-399.receipt.txt'
$taskFreshGallerySha = 'B2D045F2FB999C60B0A8C6BFEE07A14A5954B72BF3CDB3AC49CF8C0CFA2E7D8F'
function Require([bool] $condition,[string] $message) { if (-not $condition) { throw $message } }
function Assert-OriginalSeal([string] $packet,[string] $expectedSealSha) {
    $packet = [IO.Path]::GetFullPath($packet)
    $seal = Join-Path $packet 'SHA256SUMS'
    Require ((Get-FileHash -LiteralPath $seal).Hash -ceq $expectedSealSha) 'Original seal binding changed.'
    $count = 0
    foreach ($line in [IO.File]::ReadAllLines($seal)) {
        Require ($line -cmatch '^([a-f0-9]{64})  (.+)$') 'Malformed original seal.'
        $sha = $Matches[1]; $relative = $Matches[2]
        $path = [IO.Path]::GetFullPath((Join-Path $packet $relative))
        Require ($path.StartsWith($packet + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Original seal escapes packet.'
        Require ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ceq $sha) 'Original capture changed.'
        $count++
    }
    return $count
}
$actualHead = (& git -C $taskRepo rev-parse HEAD).Trim()
Require ($LASTEXITCODE -eq 0 -and $actualHead -ceq $taskHead) 'Current source changed.'
$status = @(& git -C $taskRepo status --porcelain=v1 --untracked-files=normal)
Require ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Source is dirty.'
# This historical receipt is used only as the index of already viewed exact PNG bytes.
# Its old run/head is retained literally and is not reused as current native qualification.
Require ((Get-FileHash -LiteralPath $taskHistorical).Hash -ceq $taskHistoricalSha) 'Historical visual receipt changed.'
$historical = Get-Content -LiteralPath $taskHistorical -Raw | ConvertFrom-Json -AsHashtable
Require ($historical['runId'] -eq 38166757540 -and $historical['head'] -ceq '1cdbfde4c93bafa6a0c794fd80939387467fefa4') 'Historical visual provenance differs.'
$historicalGallery = @($historical['profiles'][0]['gallery'])
Require ($historicalGallery.Count -eq 6) 'Historical six-image review differs.'
Require ((Get-FileHash -LiteralPath $taskFreshGallery).Hash -ceq $taskFreshGallerySha) 'Fresh visual receipt changed.'
$freshVisual = Get-Content -LiteralPath $taskFreshGallery -Raw | ConvertFrom-Json -AsHashtable
Require ($freshVisual['status'] -ceq 'ROOT_VIEWED_ACTUAL_NATIVE_HARNESS_PNGS' -and $freshVisual['runId'] -eq 38172114803 -and $freshVisual['head'] -ceq $taskHead -and $freshVisual['testedMerge'] -ceq $taskMerge -and $freshVisual['images'].Count -eq 6) 'Fresh visual source binding differs.'
$names = foreach ($relative in @('Tests/Controls/Scope.Tests.IntrinsicLayout.cpp','Tests/Controls/Scope.Tests.Forms.cpp','Tests/Controls/Scope.Tests.FormAccessibility.cpp')) {
    foreach ($match in [regex]::Matches([IO.File]::ReadAllText((Join-Path $taskRepo $relative)),'(?m)^void (Test\w+)\(')) { $match.Groups[1].Value }
}
$names = @($names)
Require ($names.Count -eq 38 -and @($names | Sort-Object -Unique).Count -eq 38) 'Current new test inventory differs.'
$profiles = foreach ($receipt in $ReviewReceipt) {
    $r = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
    Require ($r['status'] -ceq 'ROOT_PROFILE_REVIEW_READY_NOT_CI_GATE_SUCCESS' -and $r['runId'] -eq 38172114803 -and $r['attempt'] -eq 1 -and $r['head'] -ceq $taskHead -and $r['testedMerge'] -ceq $taskMerge -and $r['blockers'].Count -eq 0) 'Profile binding or blockers differ.'
    $sealed = Assert-OriginalSeal $r['originalCapture'] $r['originalSealSha256']
    $profile = $r['native'][0]['profile']
    Require (@($r['native'] | Where-Object { $_['profile'] -cne $profile }).Count -eq 0) 'Profile receipt mixes configurations.'
    $logs = foreach ($entry in $r['native']) {
        Require ((Get-FileHash -LiteralPath $entry['suiteLog']).Hash -ceq $entry['suiteLogSha256']) 'Reviewed suite log changed.'
        [IO.File]::ReadAllText($entry['suiteLog'])
    }
    $joined = $logs -join "`n"
    foreach ($name in $names) {
        Require ([regex]::Matches($joined,'(?m)^\s*\[DONE\] ' + [regex]::Escape($name) + ' \(').Count -eq 1) ('New test not completed exactly once: ' + $profile + '/' + $name)
    }
    $embedded = @($r['native'] | Where-Object { $_['suite'] -ceq 'Embedded' })
    Require ($embedded.Count -eq 1 -and $embedded[0]['exitCode'] -eq 0) 'Embedded result differs.'
    $embeddedText = [IO.File]::ReadAllText($embedded[0]['suiteLog'])
    Require ($embeddedText -cmatch '(?m)^PASS 4990 checks; 1000 warm composites \d+ us; C\+\+ allocations 0; changed toggle pixels \d+') 'Embedded check count or warm allocation proof differs.'
    $gallery = foreach ($entry in $r['gallery']) {
        $leaf = [IO.Path]::GetFileName($entry['file'])
        $old = @($historicalGallery | Where-Object { [IO.Path]::GetFileName($_['file']) -ceq $leaf })
        Require ($old.Count -eq 1 -and $entry['sourceCommit'] -ceq $taskMerge -and (Get-FileHash -LiteralPath $entry['file']).Hash -ceq $entry['sha256']) 'Current gallery binding differs.'
        Require ((Get-FileHash -LiteralPath $old[0]['file']).Hash -ceq $old[0]['sha256']) 'Historical viewed PNG changed.'
        $sameHistoricalPixels = $entry['sha256'] -ceq $old[0]['sha256']
        $viewed = @($freshVisual['images'] | Where-Object { [IO.Path]::GetFileName($_['file']) -ceq $leaf -and $_['sha256'] -ceq $entry['sha256'] })
        Require ($sameHistoricalPixels -or $viewed.Count -eq 1) 'New gallery needs fresh visual inspection.'
        if ($viewed.Count -eq 1) { Require ((Get-FileHash -LiteralPath $viewed[0]['file']).Hash -ceq $viewed[0]['sha256']) 'Fresh viewed PNG changed.' }
        [ordered]@{file=$entry['file'];sha256=$entry['sha256'];visual='Root-viewed harness PNG bytes; populated layout/forms, readable multilingual labels and help/error, separate actions.';sameHistoricalPixels=$sameHistoricalPixels;historicalVisualReference=$old[0]['file'];freshVisualReceipt=$(if ($sameHistoricalPixels) {$null} else {$taskFreshGallery})}
    }
    $gallery = @($gallery)
    Require ($gallery.Count -eq 6 -or ($gallery.Count -eq 0 -and $r['jobConclusion'] -ceq 'failure' -and $r['foregroundRefusalCandidates'].Count -eq 2)) 'Gallery omission lacks a failed ARM foreground candidate.'
    $postSealed = Assert-OriginalSeal $r['originalCapture'] $r['originalSealSha256']
    Require ($sealed -eq $postSealed) 'Original inventory drifted.'
    [ordered]@{profile=$profile;review=$receipt;reviewSha256=(Get-FileHash -LiteralPath $receipt).Hash;sealedFiles=$sealed;tests=$names;embeddedChecks=4990;warmComposites=1000;warmAllocations=0;gallery=$gallery;gallerySkippedAfterFailedForeground=($gallery.Count -eq 0);consumers=$r['consumers'];asanProbes=$r['asanNegativeProbes'];refusalRequiresSeparateRootBeforeInputReview=$r['foregroundRefusalCandidates']}
}
$actualHead = (& git -C $taskRepo rev-parse HEAD).Trim()
Require ($LASTEXITCODE -eq 0 -and $actualHead -ceq $taskHead) 'Current source changed during review.'
$status = @(& git -C $taskRepo status --porcelain=v1 --untracked-files=normal)
Require ($LASTEXITCODE -eq 0 -and $status.Count -eq 0) 'Source changed during review.'
$output = Join-Path $PSScriptRoot ('root-functional-visual-399-' + [guid]::NewGuid().ToString('N') + '.receipt.txt')
[ordered]@{status='ROOT_CURRENT_FUNCTIONAL_AND_VISUAL_REVIEW';runId=38172114803;attempt=1;head=$taskHead;testedMerge=$taskMerge;profiles=@($profiles);historicalVisualReceipt=$taskHistorical;historicalVisualReceiptSha256=$taskHistoricalSha;freshVisualReceipt=$taskFreshGallery;freshVisualReceiptSha256=$taskFreshGallerySha;scope='Fresh current-source new test completions and exact-image visual review only. Paired, aggregate, manual input, raw budget and individual ARM before-input review remain separate.';completedUtc=[DateTimeOffset]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $output -Encoding utf8NoBOM
Write-Output ('ROOT_FUNCTIONAL_VISUAL_REVIEW: profiles=' + @($profiles).Count + '; 38/38 completions per profile; ' + $output)
