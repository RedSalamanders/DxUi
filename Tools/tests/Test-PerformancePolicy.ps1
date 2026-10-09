[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PerformancePolicy.psm1') -Force

function Invoke-FixtureGit([string] $Root, [string[]] $Arguments) {
    & git -C $Root -c user.name=fixture -c user.email=fixture@example.invalid -c commit.gpgsign=false @Arguments 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed" }
}

Invoke-FixtureCase 'only an independently qualified policy present in the measured base is trusted' {
    param($root)
    Invoke-FixtureGit $root @('init','-q','-b','main')
    $judge = "function Get-PairedPerformanceJudgeVersion { 'dxui-fixture-v1' }`nExport-ModuleMember -Function Get-PairedPerformanceJudgeVersion`n"
    Set-FixtureFile $root 'Tools/PerformanceComparison.psm1' $judge
    $normalizedJudge = [IO.File]::ReadAllText((Join-Path $root 'Tools/PerformanceComparison.psm1')) -replace "`r`n", "`n"
    $judgeShaHasher = [Security.Cryptography.SHA256]::Create()
    try { $judgeSha = [Convert]::ToHexString($judgeShaHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($normalizedJudge))) } finally { $judgeShaHasher.Dispose() }
    $policy = [ordered]@{ schemaVersion=1; qualificationStatus='qualified'; qualified=$true; policyId='fixture-v1'; judgeVersion='dxui-fixture-v1'; approvedJudgeSha256=$judgeSha }
    $path = Join-Path $root 'Tools/PerformanceAcceptancePolicy.v1.json'
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    $json = ConvertTo-Json -InputObject $policy -Depth 4
    [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
    Invoke-FixtureGit $root @('add','-A'); Invoke-FixtureGit $root @('commit','-q','-m','qualified fixture policy')
    $commit = (& git -C $root rev-parse HEAD).Trim()
    $trusted = Get-TrustedPerformancePolicy -BaselineCommit $commit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True $trusted.trusted "base policy qualifies and candidate policy is semantically unchanged: $($trusted.reason)"
    Assert-Equal 'trusted' $trusted.status 'trusted status'
    Assert-True ($trusted.policySha256 -match '^[A-F0-9]{64}$') 'policy hash retained'
    $unapprovedJudge = Copy-JsonValue $policy
    $unapprovedJudge.approvedJudgeSha256 = ('0' * 64)
    [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $unapprovedJudge -Depth 4), [Text.UTF8Encoding]::new($false))
    $unapprovedJudgeResult = Get-TrustedPerformancePolicy -BaselineCommit $commit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True (-not $unapprovedJudgeResult.trusted) 'a candidate cannot change its approved judge hash'
    Assert-True $unapprovedJudgeResult.reason.Contains('candidate changes the trusted policy') 'candidate judge approval mutation is explicit'
    [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $unapprovedJudge -Depth 4), [Text.UTF8Encoding]::new($false))
    Invoke-FixtureGit $root @('add','-A'); Invoke-FixtureGit $root @('commit','-q','-m','wrong judge approval')
    $wrongApprovalCommit = (& git -C $root rev-parse HEAD).Trim()
    $wrongApproval = Get-TrustedPerformancePolicy -BaselineCommit $wrongApprovalCommit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True (-not $wrongApproval.trusted) 'a base cannot trust a policy hash for another judge source'
    Assert-True $wrongApproval.reason.Contains('does not match the hash approved') 'measured-base source mismatch is explicit'
    $wrongVersionPolicy = Copy-JsonValue $policy
    $wrongVersionPolicy.judgeVersion = 'dxui-other-v1'
    [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $wrongVersionPolicy -Depth 4), [Text.UTF8Encoding]::new($false))
    Invoke-FixtureGit $root @('add','-A'); Invoke-FixtureGit $root @('commit','-q','-m','wrong judge version')
    $wrongVersionCommit = (& git -C $root rev-parse HEAD).Trim()
    $wrongVersion = Get-TrustedPerformancePolicy -BaselineCommit $wrongVersionCommit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True (-not $wrongVersion.trusted) 'a base cannot trust a policy version that differs from its judge source'
    Assert-True $wrongVersion.reason.Contains('version does not match') 'measured-base version mismatch is explicit'
    [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
    $mutated = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json -AsHashtable
    $mutated.policyId = 'candidate-v2'
    [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $mutated -Depth 4), [Text.UTF8Encoding]::new($false))
    $changed = Get-TrustedPerformancePolicy -BaselineCommit $commit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True (-not $changed.trusted) 'candidate policy mutation cannot inherit trust'
    Assert-Equal 'policy-review-required' $changed.status 'mutated policy status'
}

Invoke-FixtureCase 'a missing or unqualified base policy cannot be bootstrapped by the candidate' {
    param($root)
    Invoke-FixtureGit $root @('init','-q','-b','main')
    Set-FixtureFile $root 'README.md' "baseline`n"
    Invoke-FixtureGit $root @('add','-A'); Invoke-FixtureGit $root @('commit','-q','-m','base without policy')
    $commit = (& git -C $root rev-parse HEAD).Trim()
    $candidate = Join-Path $root 'Tools/PerformanceAcceptancePolicy.v1.json'
    Set-FixtureFile $root 'Tools/PerformanceAcceptancePolicy.v1.json' '{"schemaVersion":1,"qualified":true,"qualificationStatus":"qualified"}'
    $missing = Get-TrustedPerformancePolicy -BaselineCommit $commit -RepositoryRoot $root -CandidatePolicyPath $candidate
    Assert-True (-not $missing.trusted) 'candidate cannot add the trust anchor'
    Assert-Equal 'policy-review-required' $missing.status 'missing base policy remains pending'
    Assert-True $missing.reason.Contains('cannot introduce its own trust anchor') 'reason explains the rule'
}

Invoke-TestCase 'receipt identity reports every missing provenance element' {
    $receipt = [ordered]@{ sourceCommit='a'; sourceFingerprint='b'; executableSha256='c'; benchmarkSha256='d'; harnessIdentity='e'; toolchainIdentity='f'; dependencyIdentity='g'; environmentIdentity='env'; identityStatus='verifiable';
        fixture='f'; renderer='r'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5; platform='x64'; configuration='Release'; nativeArchitecture='X64';
        machine='h'; cpu='i'; os='j'; compiler='cl'; powerPolicy='k'; warpVersion='l'; warpSha256=('A' * 64) }
    Assert-True (Test-PerformanceReceiptIdentity $receipt).verifiable 'complete identity is verifiable'
    $receipt.Remove('toolchainIdentity')
    $result = Test-PerformanceReceiptIdentity $receipt
    Assert-True (-not $result.verifiable) 'missing toolchain is inconclusive evidence'
    Assert-Equal 'toolchainIdentity' ($result.missing -join ',') 'the missing field is named'
    $receipt.toolchainIdentity = 'f'
    $receipt.identityStatus = 'identity-unverifiable'
    $unverifiable = Test-PerformanceReceiptIdentity $receipt
    Assert-True (-not $unverifiable.verifiable) 'a populated but explicitly unresolved identity remains inconclusive'
    Assert-True ($unverifiable.missing -contains 'identityStatus') 'unverifiable identity status is reported'
    $receipt.Remove('warpSha256')
    Assert-True ((Test-PerformanceReceiptIdentity $receipt).missing -contains 'warpSha256') 'the WARP binary hash is required provenance'
}

Invoke-TestCase 'platform-only identity cannot qualify a benchmark receipt and Full still requires the harness' {
    $receipt = [ordered]@{ sourceCommit='a'; sourceFingerprint='b'; executableSha256='c'; benchmarkSha256='d'; harnessIdentity='e'; toolchainIdentity='f'; dependencyIdentity='g'; environmentIdentity='env'; identityStatus='platform-only'; identityScope='platform-only';
        fixture='f'; renderer='r'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5; platform='x64'; configuration='Release'; nativeArchitecture='X64';
        machine='h'; cpu='i'; os='j'; compiler='cl'; powerPolicy='k'; warpVersion='l'; warpSha256=('A' * 64) }
    $platformResult = Test-PerformanceReceiptIdentity $receipt
    Assert-True (-not $platformResult.verifiable) 'platform-only status is not a benchmark identity'
    Assert-True ($platformResult.missing -contains 'identityStatus' -and $platformResult.missing -contains 'identityScope') 'both status and scope prevent receipt qualification'
    $receipt.identityStatus = 'verifiable'
    $relabeledResult = Test-PerformanceReceiptIdentity $receipt
    Assert-True (-not $relabeledResult.verifiable -and $relabeledResult.missing -contains 'identityScope') 'a relabeled platform identity still cannot qualify'

}

Invoke-FixtureCase 'Full identity requires the harness while PlatformOnly skips only harness hashing' {
    param($root)
    foreach ($relative in @('performance.ps1','performance-paired.ps1','build.ps1','vcpkg-install.ps1',
            'Tools/Compare-Performance.ps1','Tools/PerformanceComparison.psm1','Tools/PairedRun.psm1','Tools/BenchmarkGate.psm1','Tools/PerformancePolicy.psm1',
            'Tools/VisualStudio.psm1','Tools/VcpkgTriplet.psm1','vcpkg.json','vcpkg-tool.json','src/DxUi.vcxproj')) {
        Set-FixtureFile $root $relative 'fixture input'
    }
    Set-FixtureFile $root 'Tools/VisualStudio.psm1' @'
function Get-DxUiVisualStudioInstallation { Join-Path $PSScriptRoot 'missing-visual-studio-fixture' }
Export-ModuleMember -Function Get-DxUiVisualStudioInstallation
'@
    $full = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Debug
    Assert-Equal 'full' $full.identityScope 'default identity scope remains full'
    Assert-Equal 'identity-unverifiable' $full.identityStatus 'default full identity fails closed'
    Assert-True $full.identityError.Contains('Tools/PerformanceAcceptancePolicy.v1.json') 'full identity still requires the accepted harness input'

    $platform = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Debug -IdentityScope PlatformOnly
    Assert-Equal 'platform-only' $platform.identityScope 'the partial identity is explicitly scoped'
    Assert-Equal 'identity-unverifiable' $platform.identityStatus 'missing platform toolchain still fails closed'
    Assert-True $platform.identityError.Contains('MSBuild identity is unavailable') 'platform-only skips only harness hashing and reaches platform verification'
    Assert-True (-not $platform.identityError.Contains('PerformanceAcceptancePolicy.v1.json')) 'a missing policy is not mistaken for platform drift'
}

Invoke-TestCase 'expanded MSBuild project identity normalizes only roots and line endings and validates XML' {
    $candidateRoot = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-candidate-fixture'
    $baselineRoot = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-baseline-fixture'
    $candidate = @(
        '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
        "  <!-- imported from '$candidateRoot\Directory.Build.props' -->",
        '  <PropertyGroup><FixtureOption>alpha</FixtureOption></PropertyGroup>',
        "  <!-- imported from '$candidateRoot\Microsoft.Cpp.props' -->",
        '  <ItemDefinitionGroup><ClCompile><AdditionalOptions>/utf-8</AdditionalOptions></ClCompile></ItemDefinitionGroup>',
        '</Project>'
    ) -join "`n"
    $baseline = $candidate.Replace($candidateRoot, $baselineRoot)
    $candidateIdentity = Get-PreprocessedProjectIdentity -Text $candidate -Root $candidateRoot
    $baselineIdentity = Get-PreprocessedProjectIdentity -Text ($baseline -replace "`n", "`r`n") -Root $baselineRoot
    Assert-Equal $candidateIdentity $baselineIdentity 'checkout paths and line endings do not change the expanded project hash'
    $alternateRootSpelling = $candidate.Replace($candidateRoot, $candidateRoot.Replace('\','/'))
    Assert-Equal $candidateIdentity (Get-PreprocessedProjectIdentity -Text $alternateRootSpelling -Root $candidateRoot) 'both exact root spellings share the existing path normalizer'

    $changedContent = $baseline.Replace('<FixtureOption>alpha</FixtureOption>', '<FixtureOption>beta</FixtureOption>')
    $changedContentIdentity = Get-PreprocessedProjectIdentity -Text $changedContent -Root $baselineRoot
    Assert-True ($changedContentIdentity -cne $baselineIdentity) 'an imported project content change changes identity'
    $firstImport = "  <!-- imported from '$baselineRoot\Directory.Build.props' -->`n  <PropertyGroup><FixtureOption>alpha</FixtureOption></PropertyGroup>"
    $secondImport = "  <!-- imported from '$baselineRoot\Microsoft.Cpp.props' -->`n  <ItemDefinitionGroup><ClCompile><AdditionalOptions>/utf-8</AdditionalOptions></ClCompile></ItemDefinitionGroup>"
    $reordered = "<Project xmlns=`"http://schemas.microsoft.com/developer/msbuild/2003`">`n$secondImport`n$firstImport`n</Project>"
    $reorderedIdentity = Get-PreprocessedProjectIdentity -Text $reordered -Root $baselineRoot
    Assert-True ($reorderedIdentity -cne $baselineIdentity) 'import order changes identity'
    Assert-Throws { Get-PreprocessedProjectIdentity -Text '' -Root $baselineRoot } 'empty preprocessing output is rejected'
    Assert-Throws { Get-PreprocessedProjectIdentity -Text '<Project><PropertyGroup></Project>' -Root $baselineRoot } 'malformed XML is rejected'
    Assert-Throws { Get-PreprocessedProjectIdentity -Text '<Target />' -Root $baselineRoot } 'a valid non-Project XML root is rejected'
}

Invoke-TestCase 'resolved compiler identity is stable across equivalent source roots and changes with effective options' {
    $leftRoot = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-left-fixture'
    $rightRoot = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-right-fixture'
    $leftItems = @([ordered]@{ Identity='Controls\Panel.cpp'; AdditionalIncludeDirectories="$leftRoot\include;$leftRoot\.build\generated";
        AdditionalOptions='/utf-8 /Zc:preprocessor'; PreprocessorDefinitions='UNICODE;NOMINMAX'; LanguageStandard='stdcpplatest';
        RuntimeLibrary='MultiThreadedDLL'; Optimization='MaxSpeed'; FullPath="$leftRoot\src\Controls\Panel.cpp"; ModifiedTime='2026-10-08T10:00:00Z' })
    $rightItems = @([ordered]@{ Identity='Controls\Panel.cpp'; AdditionalIncludeDirectories="$rightRoot\include;$rightRoot\.build\generated";
        AdditionalOptions='/utf-8 /Zc:preprocessor'; PreprocessorDefinitions='UNICODE;NOMINMAX'; LanguageStandard='stdcpplatest';
        RuntimeLibrary='MultiThreadedDLL'; Optimization='MaxSpeed'; FullPath="$rightRoot\src\Controls\Panel.cpp"; ModifiedTime='2026-10-09T11:30:00Z' })
    $leftIdentity = Get-ResolvedCompileItemsIdentity -Items $leftItems -Root $leftRoot
    $rightIdentity = Get-ResolvedCompileItemsIdentity -Items $rightItems -Root $rightRoot
    Assert-Equal $leftIdentity $rightIdentity 'absolute worktree paths and filesystem timestamps do not affect effective compiler identity'
    $rightItems[0].AdditionalOptions = '/utf-8 /Zc:preprocessor /DADDED'
    $changedOptions = Get-ResolvedCompileItemsIdentity -Items $rightItems -Root $rightRoot
    Assert-True ($changedOptions -cne $leftIdentity) 'a real compiler option change changes identity'
}

Invoke-TestCase 'the migrated acceptance policy describes the implemented family and exact peak rules without self-qualification' {
    $policyPath = Join-Path $PSScriptRoot '../PerformanceAcceptancePolicy.v1.json'
    $policy = Get-Content -LiteralPath $policyPath -Raw | ConvertFrom-Json
    Assert-Equal $false $policy.qualified 'the policy remains explicitly unqualified'
    Assert-True $policy.primaryFamily.Contains('26 predeclared phase-metric slots per benchmark scenario') 'the Holm family matches the judge'
    Assert-True $policy.exactBudgetRule.Contains('raw candidate-round maximum above the retained baseline raw-round maximum') 'the exact peak gate is documented'
    Assert-True $policy.exactBudgetRule.Contains('candidate run median above the baseline run-median median') 'the exact median gate is preserved'
    Assert-True (@($policy.requiresFullIdentity) -contains 'environmentIdentity') 'the policy requires resolved runner/build identity'
    Assert-True (@($policy.requiresFullIdentity) -contains 'warpSha256') 'the policy requires the WARP binary hash'
    Assert-Equal 'dxui-paired-block-sign-flip-holm-v1' $policy.judgeVersion 'the policy names the judge version'
    Assert-True ($policy.approvedJudgeSha256 -match '^[A-Fa-f0-9]{64}$') 'the proposed judge source hash is reviewable'
}
Complete-TestRun 'PerformancePolicy'
