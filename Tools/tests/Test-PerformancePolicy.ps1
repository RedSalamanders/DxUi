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
    $judge = "function Get-PairedPerformanceJudgeVersion { 'dxui-fixture-v1' }`nfunction Get-PairedAssignmentProtocol { 'fixture-independent-v1' }`nExport-ModuleMember -Function Get-PairedPerformanceJudgeVersion, Get-PairedAssignmentProtocol`n"
    Set-FixtureFile $root 'Tools/PerformanceComparison.psm1' $judge
    $normalizedJudge = [IO.File]::ReadAllText((Join-Path $root 'Tools/PerformanceComparison.psm1')) -replace "`r`n", "`n"
    $judgeShaHasher = [Security.Cryptography.SHA256]::Create()
    try { $judgeSha = [Convert]::ToHexString($judgeShaHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($normalizedJudge))) } finally { $judgeShaHasher.Dispose() }
    $assignmentPath = Join-Path $root 'Tools/PairedRun.psm1'
    Set-FixtureFile $root 'Tools/PairedRun.psm1' "# independently approved assignment fixture`n"
    $assignmentSource = [IO.File]::ReadAllText($assignmentPath) -replace "`r`n", "`n"
    $assignmentHasher = [Security.Cryptography.SHA256]::Create()
    try { $assignmentSha = [Convert]::ToHexString($assignmentHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($assignmentSource))) }
    finally { $assignmentHasher.Dispose() }
    $policy = [ordered]@{ schemaVersion=1; qualificationStatus='qualified'; qualified=$true; policyId='fixture-v1'; judgeVersion='dxui-fixture-v1'; approvedJudgeSha256=$judgeSha;
        assignmentProtocol='fixture-independent-v1'; approvedAssignmentSha256=$assignmentSha }
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
    Set-FixtureFile $root 'Tools/PairedRun.psm1' "# candidate silently changes the assignment semantics`n"
    $changedAssignment = Get-TrustedPerformancePolicy -BaselineCommit $commit -RepositoryRoot $root -CandidatePolicyPath $path
    Assert-True (-not $changedAssignment.trusted) 'an unchanged judge and protocol string cannot approve another candidate assignment generator'
    Assert-True $changedAssignment.reason.Contains('changes the approved assignment generator') 'assignment mutation requires review'
    [IO.File]::WriteAllText($assignmentPath, $assignmentSource, [Text.UTF8Encoding]::new($false))
    foreach ($mutation in @('missingHash', 'wrongHash', 'wrongProtocol')) {
        $wrongAssignmentPolicy = Copy-JsonValue $policy
        switch ($mutation) {
            'missingHash' { $wrongAssignmentPolicy.Remove('approvedAssignmentSha256') }
            'wrongHash' { $wrongAssignmentPolicy.approvedAssignmentSha256 = ('0' * 64) }
            'wrongProtocol' { $wrongAssignmentPolicy.assignmentProtocol = 'another-protocol-v1' }
        }
        [IO.File]::WriteAllText($path, (ConvertTo-Json -InputObject $wrongAssignmentPolicy -Depth 4), [Text.UTF8Encoding]::new($false))
        Invoke-FixtureGit $root @('add','-A'); Invoke-FixtureGit $root @('commit','-q','-m',"wrong assignment $mutation")
        $wrongAssignmentCommit = (& git -C $root rev-parse HEAD).Trim()
        $wrongAssignment = Get-TrustedPerformancePolicy -BaselineCommit $wrongAssignmentCommit -RepositoryRoot $root -CandidatePolicyPath $path
        Assert-True (-not $wrongAssignment.trusted) "the base cannot qualify $mutation assignment metadata"
        Assert-True $wrongAssignment.reason.Contains('assignment') 'assignment qualification failure is explicit'
    }
    [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
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
    $receipt = [ordered]@{ sourceCommit='a'; sourceFingerprint='b'; executableSha256='c'; benchmarkSha256='d'; harnessIdentity='e'; toolchainIdentity='f'; dependencyIdentity='g'; environmentIdentity='env'; identityStatus='verifiable'; identityScope='full';
        buildIdentityVersion=2; resolvedBuildIdentity=('1' * 64); buildInputIdentity=('2' * 64); buildArtifactIdentity=('3' * 64);
        fixture='f'; renderer='r'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5; platform='x64'; configuration='Release'; nativeArchitecture='X64';
        machine='h'; cpu='i'; os='j'; compiler='cl'; powerPolicy='k'; warpVersion='l'; warpSha256=('A' * 64) }
    $receipt['buildAttestationIdentity'] = Get-PerformanceBuildAttestationIdentity -State $receipt -ArtifactIdentity $receipt['buildArtifactIdentity']
    Assert-True (Test-PerformanceReceiptIdentity $receipt).verifiable 'complete identity is structurally verifiable, not proof of a real fixture build'
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

    $build = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Debug -IdentityScope Build
    Assert-Equal 'build-only' $build.identityScope 'build identity has a separate limited scope'
    Assert-Equal 'identity-unverifiable' $build.identityStatus 'missing build compiler identity fails closed'
    Assert-True $build.identityError.Contains('MSBuild identity is unavailable') 'fresh build attestation does not require a benchmark output/harness'
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
        RuntimeLibrary='MultiThreadedDLL'; Optimization='MaxSpeed'; WarningLevel='Level4'; TreatWarningAsError='true'; SDLCheck='true'; ConformanceMode='true'; ExceptionHandling='Sync';
        FullPath="$leftRoot\src\Controls\Panel.cpp"; ModifiedTime='2026-10-08T10:00:00Z' })
    $rightItems = @([ordered]@{ Identity='Controls\Panel.cpp'; AdditionalIncludeDirectories="$rightRoot\include;$rightRoot\.build\generated";
        AdditionalOptions='/utf-8 /Zc:preprocessor'; PreprocessorDefinitions='UNICODE;NOMINMAX'; LanguageStandard='stdcpplatest';
        RuntimeLibrary='MultiThreadedDLL'; Optimization='MaxSpeed'; WarningLevel='Level4'; TreatWarningAsError='true'; SDLCheck='true'; ConformanceMode='true'; ExceptionHandling='Sync';
        FullPath="$rightRoot\src\Controls\Panel.cpp"; ModifiedTime='2026-10-09T11:30:00Z' })
    $leftIdentity = Get-ResolvedCompileItemsIdentity -Items $leftItems -Root $leftRoot
    $rightIdentity = Get-ResolvedCompileItemsIdentity -Items $rightItems -Root $rightRoot
    Assert-Equal $leftIdentity $rightIdentity 'absolute worktree paths and filesystem timestamps do not affect effective compiler identity'
    $rightItems[0].AdditionalOptions = '/utf-8 /Zc:preprocessor /DADDED'
    $changedOptions = Get-ResolvedCompileItemsIdentity -Items $rightItems -Root $rightRoot
    Assert-True ($changedOptions -cne $leftIdentity) 'a real compiler option change changes identity'
}

function New-PerformanceProjectFixture([string] $Root, [string] $Role = 'library') {
    $text = @'
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><FixtureFlag>/fixture0</FixtureFlag></PropertyGroup>
  <ItemGroup><ClCompile Include="Controls\One.cpp" /><ClCompile Include="Controls\Two.cpp" /></ItemGroup>
  <Target Name="Compile" Condition="'$(ConditionalFlag)' != ''"><ItemGroup><ClCompile><Optimization Condition="'$(ConditionalFlag)' == 'optimize'">MaxSpeed</Optimization></ClCompile></ItemGroup><CL AdditionalOptions="$(FixtureFlag)" /></Target>
  <UsingTask TaskName="CL" AssemblyFile="fixture-tasks.dll" />
</Project>
'@
    $module = Get-Module PerformancePolicy
    $names = & $module { param($Text) Get-PerformanceToolPropertyNames ([xml]$Text) } $text
    $properties = [ordered]@{}
    foreach ($name in $names) { $properties[$name] = '' }
    foreach ($entry in ([ordered]@{ PlatformToolset='v145'; VCToolsInstallDir="$Root/toolchain/msvc/"; VCToolsVersion='14.51'; WindowsSdkDir="$Root/toolchain/sdk/";
            WindowsTargetPlatformVersion='10.0'; PreferredToolArchitecture='x86'; DxUiOutputRoot="$Root/.build/"; VCTargetsPath="$Root/toolchain/targets/";
            ConfigurationType=$(if ($Role -eq 'library') { 'StaticLibrary' } else { 'Application' }); VCToolArchitecture='Native32Bit'; WindowsSDKToolArchitecture='Native32Bit';
            IncludePath="$Root/include;$Root/toolchain/msvc/include"; LibraryPath="$Root/toolchain/msvc/lib/x64;$Root/toolchain/sdk/Lib/10.0/um/x64";
            IntDir="$Root/.build/Intermediate/x64/Release/$(if ($Role -eq 'library') { 'DxUi' } else { 'DxUi.EmbeddedTests' })/";
            OutDir="$Root/.build/x64/Release/"; VSInstallDir="$Root/toolchain/vs/"; UniversalCRTSdkDir="$Root/toolchain/sdk/"; UCRTVersion='10.0'; NETFXKitsDir="$Root/toolchain/netfx/";
            ExecutablePath="$Root/toolchain/bin"; FixtureFlag='/fixture0' }).GetEnumerator()) {
        $properties[$entry.Key] = $entry.Value
    }
    $clItems = @('Controls\One.cpp','Controls\Two.cpp' | ForEach-Object {
        [ordered]@{ Identity=$_; FullPath="$Root/$(if ($Role -eq 'library') { 'src' } else { 'Tests/Embedded' })/$_"; AdditionalIncludeDirectories="$Root/include"; AdditionalOptions='/utf-8 /Zc:preprocessor';
            PreprocessorDefinitions='UNICODE;NOMINMAX'; LanguageStandard='stdcpplatest'; RuntimeLibrary='MultiThreadedDLL'; Optimization='MaxSpeed';
            WarningLevel='Level4'; TreatWarningAsError='true'; SDLCheck='true'; ConformanceMode='true'; ExceptionHandling='Sync';
            ObjectFileName=$properties.IntDir; CustomCompilerFlag='unchanged' }
    })
    $fx = @()
    if ($Role -eq 'library') {
        foreach ($suffix in @('VS','PS')) {
            $fx += [ordered]@{ Identity="Rendering\Composite$suffix.hlsl"; FullPath="$Root/src/Rendering/Composite$suffix.hlsl"; ShaderType=$(if ($suffix -eq 'VS') { 'Vertex' } else { 'Pixel' });
                ShaderModel='5.0'; EntryPointName="${suffix}Main"; HeaderFileOutput="$($properties.IntDir)DxUiComposite$suffix.h"; ObjectFileOutput="$($properties.IntDir)DxUiComposite$suffix.cso";
                VariableName="g_Composite$suffix"; AdditionalOptions='/Ges /O3 /Qstrip_debug'; EnableDebuggingInformation='false' }
        }
    }
    $output = "$($properties.OutDir)$(if ($Role -eq 'library') { 'DxUi.lib' } else { 'DxUi.EmbeddedTests.exe' })"
    $items = [ordered]@{ ClCompile=$clItems; FxCompile=$fx;
        Link=@([ordered]@{ Identity='DxUiPerformanceLinkProbe'; OutputFile=$output; TargetMachine='MachineX64'; AdditionalOptions=''; OptimizeReferences='true'; SubSystem='Console' });
        Lib=@([ordered]@{ Identity='DxUiPerformanceLibProbe'; OutputFile=$output; TargetMachine='MachineX64'; AdditionalOptions='' });
        ProjectReference=@();
        CustomBuild=@(); ResourceCompile=@(); Midl=@(); MASM=@(); ARMASM=@(); BuildMacro=@();
        PreBuildEvent=@([ordered]@{ Identity='DxUiPerformancePreBuildEventProbe'; Command='' });
        PreLinkEvent=@([ordered]@{ Identity='DxUiPerformancePreLinkEventProbe'; Command='' });
        PostBuildEvent=@([ordered]@{ Identity='DxUiPerformancePostBuildEventProbe'; Command='' }) }
    if ($Role -eq 'benchmark') { $items.ProjectReference = @([ordered]@{ Identity='../../src/DxUi.vcxproj'; LinkLibraryDependencies='true' }) }
    return [ordered]@{ root=$Root; role=$Role; text=$text; properties=$properties; items=$items }
}

function Get-FixtureProjectProfile([Collections.IDictionary] $Fixture) {
    return Get-PerformanceProjectProfile -Root $Fixture.root -Role $Fixture.role -ExpandedText $Fixture.text -Properties $Fixture.properties -Items $Fixture.items
}

Invoke-TestCase 'source topology changes per-side identities without changing a uniform compiler profile' {
    $root = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-project-topology-fixture'
    $before = New-PerformanceProjectFixture $root
    $original = Get-FixtureProjectProfile $before
    $added = Copy-JsonValue $before
    $third = Copy-JsonValue $added.items.ClCompile[0]
    $third.Identity = 'Controls\Three.cpp'; $third.FullPath = "$root/src/Controls/Three.cpp"
    $added.items.ClCompile += $third
    $added.text = $added.text.Replace('<ClCompile Include="Controls\Two.cpp" />', '<ClCompile Include="Controls\Two.cpp" /><ClCompile Include="Controls\Three.cpp" />')
    $reordered = Copy-JsonValue $before
    $reordered.items.ClCompile = @($reordered.items.ClCompile[1], $reordered.items.ClCompile[0])
    $reordered.text = $reordered.text.Replace('One.cpp', 'Placeholder.cpp').Replace('Two.cpp', 'One.cpp').Replace('Placeholder.cpp', 'Two.cpp')
    $removed = Copy-JsonValue $before
    $removed.items.ClCompile = @($removed.items.ClCompile[0])
    $removed.text = $removed.text.Replace('<ClCompile Include="Controls\Two.cpp" />', '')
    foreach ($changed in @($added,$reordered,$removed)) {
        $profile = Get-FixtureProjectProfile $changed
        Assert-Equal $original.profileIdentity $profile.profileIdentity 'registration alone preserves compiler/SDK/options comparability'
        Assert-True ($original.expandedProjectIdentity -cne $profile.expandedProjectIdentity) 'full expanded inventory and order remain attested per side'
    }
    Assert-True ($original.resolvedItemsIdentity -cne (Get-FixtureProjectProfile $added).resolvedItemsIdentity) 'added source has a distinct path-keyed resolved inventory'
    Assert-True ($original.resolvedItemsIdentity -cne (Get-FixtureProjectProfile $removed).resolvedItemsIdentity) 'removed source has a distinct path-keyed resolved inventory'
    $otherRoot = Copy-JsonValue $before
    $otherRoot.root = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-equivalent-project-fixture'
    $otherRoot = New-PerformanceProjectFixture $otherRoot.root
    Assert-Equal $original.profileIdentity (Get-FixtureProjectProfile $otherRoot).profileIdentity 'equivalent worktree roots have the same compiler profile'
}

Invoke-TestCase 'global and per-file compiler flags are bound without a lossy profile set or metadata whitelist' {
    foreach ($role in @('library','benchmark')) {
        $fixture = New-PerformanceProjectFixture (Join-Path ([IO.Path]::GetTempPath()) 'DxUi-options-fixture') $role
        $original = Get-FixtureProjectProfile $fixture
        foreach ($name in @('AdditionalOptions','Optimization','PreprocessorDefinitions','RuntimeLibrary','ConformanceMode','CustomCompilerFlag')) {
            $global = Copy-JsonValue $fixture
            foreach ($item in $global.items.ClCompile) { $item[$name] += ' changed' }
            Assert-True ($original.profileIdentity -cne (Get-FixtureProjectProfile $global).profileIdentity) "global $role $name is compared"
            $perFile = Copy-JsonValue $fixture
            $perFile.items.ClCompile[0][$name] += ' changed'
            Assert-Throws { Get-FixtureProjectProfile $perFile } "heterogeneous $role $name was accepted"
        }
        $swap = Copy-JsonValue $fixture
        $swap.items.ClCompile[0].Optimization = 'Disabled'
        Assert-Throws { Get-FixtureProjectProfile $swap } 'the first heterogeneous flag assignment is unsupported'
        $firstIdentity = Get-ResolvedCompileItemsIdentity $swap.items.ClCompile $swap.root
        $swap.items.ClCompile[0].Optimization = 'MaxSpeed'; $swap.items.ClCompile[1].Optimization = 'Disabled'
        Assert-Throws { Get-FixtureProjectProfile $swap } 'swapping existing optimization profiles cannot manufacture uniform comparability'
        Assert-True ($firstIdentity -cne (Get-ResolvedCompileItemsIdentity $swap.items.ClCompile $swap.root)) 'a flag swap remains bound to each source path'
    }
}

Invoke-TestCase 'shader roles, linker and librarian flags and target-time overrides participate in comparison' {
    $fixture = New-PerformanceProjectFixture (Join-Path ([IO.Path]::GetTempPath()) 'DxUi-shader-link-fixture')
    $original = Get-FixtureProjectProfile $fixture
    foreach ($kind in @('FxCompile','Link','Lib')) {
        $changed = Copy-JsonValue $fixture
        $changed.items[$kind][0].AdditionalOptions += ' /changed'
        Assert-True ($original.profileIdentity -cne (Get-FixtureProjectProfile $changed).profileIdentity) "$kind flags are compared"
        Assert-True ($original.resolvedItemsIdentity -cne (Get-FixtureProjectProfile $changed).resolvedItemsIdentity) "$kind flags are separately attested"
    }
    $swapped = Copy-JsonValue $fixture
    $swapped.items.FxCompile[0].ShaderType = 'Pixel'; $swapped.items.FxCompile[1].ShaderType = 'Vertex'
    Assert-Throws { Get-FixtureProjectProfile $swapped } 'shader kinds cannot move between explicit composite roles'
    foreach ($name in @('FixtureFlag','ConditionalFlag','LinkIncremental','IncludePath','LibraryPath')) {
        $changed = Copy-JsonValue $fixture
        $changed.properties[$name] += ' changed'
        Assert-True ($original.profileIdentity -cne (Get-FixtureProjectProfile $changed).profileIdentity) "resolved global $name is compared"
    }
    foreach ($replacement in @('Target Name="Compile" BeforeTargets="ClCompile"','UsingTask TaskName="CL" AssemblyFile="changed-tasks.dll"')) {
        $changed = Copy-JsonValue $fixture
        if ($replacement.StartsWith('Target')) { $changed.text = $changed.text.Replace('Target Name="Compile"', $replacement) }
        else { $changed.text = $changed.text.Replace('UsingTask TaskName="CL" AssemblyFile="fixture-tasks.dll"', $replacement) }
        Assert-True ($original.profileIdentity -cne (Get-FixtureProjectProfile $changed).profileIdentity) 'target/task definition mutations cannot hide behind evaluation-only options'
    }
}

Invoke-TestCase 'missing metadata and unsupported build paths cannot claim full compiler profile evidence' {
    $fixture = New-PerformanceProjectFixture (Join-Path ([IO.Path]::GetTempPath()) 'DxUi-missing-build-metadata-fixture')
    foreach ($name in @('LanguageStandard','AdditionalOptions','Optimization','RuntimeLibrary','WarningLevel','ConformanceMode','PreprocessorDefinitions')) {
        $changed = Copy-JsonValue $fixture
        [void]$changed.items.ClCompile[0].Remove($name)
        Assert-Throws { Get-FixtureProjectProfile $changed } "missing ClCompile $name was accepted"
    }
    foreach ($name in @('ShaderModel','EntryPointName','ShaderType','AdditionalOptions')) {
        $changed = Copy-JsonValue $fixture
        [void]$changed.items.FxCompile[0].Remove($name)
        Assert-Throws { Get-FixtureProjectProfile $changed } "missing FxCompile $name was accepted"
    }
    foreach ($kind in @('Link','Lib','FxCompile','ClCompile')) {
        $changed = Copy-JsonValue $fixture
        [void]$changed.items.Remove($kind)
        Assert-Throws { Get-FixtureProjectProfile $changed } "missing $kind inventory was accepted"
    }
    $changed = Copy-JsonValue $fixture
    $changed.properties.ForceImportAfterCppTargets = 'actual-project-customization.targets'
    Assert-Throws { Get-FixtureProjectProfile $changed } 'a real forced-after customization was overwritten'
    $changed = Copy-JsonValue $fixture
    [void]$changed.properties.Remove('VCTargetsPath')
    Assert-Throws { Get-FixtureProjectProfile $changed } 'missing resolved imported targets path was accepted'
    $changed = Copy-JsonValue $fixture
    $changed.items.CustomBuild = @([ordered]@{ Identity='unattested-generator' })
    Assert-Throws { Get-FixtureProjectProfile $changed } 'an unattested custom compilation path was accepted'
    $changed = Copy-JsonValue $fixture
    $changed.items.PreBuildEvent[0].Command = 'mutate generated source'
    Assert-Throws { Get-FixtureProjectProfile $changed } 'an unsupported source-mutating build event was accepted'
}

Invoke-TestCase 'a metadata probe cannot hide real compiler shader reference or generator options' {
    $root = Join-Path ([IO.Path]::GetTempPath()) 'DxUi-probe-options-fixture'
    $fixture = New-PerformanceProjectFixture $root benchmark
    $kinds = @('ClCompile','FxCompile','ProjectReference','CustomBuild','ResourceCompile','Midl','MASM','ARMASM','BuildMacro')
    $module = Get-Module PerformancePolicy
    $assert = { param($Original,$Probed) & $module { param($Original,$Probed,$Kinds,$Root) Assert-PerformanceProbeItems $Original $Probed $Kinds $Root } $Original $Probed $kinds $root }
    & $assert $fixture.items (Copy-JsonValue $fixture.items)
    foreach ($kind in @('ClCompile','ProjectReference')) {
        $probed = Copy-JsonValue $fixture.items
        $probed[$kind][0]['AdditionalOptions'] = '/hidden-under-ForceImportAfterCppTargets'
        Assert-Throws { & $assert $fixture.items $probed } "the probe concealed actual $kind flags"
    }
    $library = New-PerformanceProjectFixture $root
    $probed = Copy-JsonValue $library.items
    $probed.FxCompile[0].ShaderModel = '6.0'
    Assert-Throws { & $assert $library.items $probed } 'the probe concealed actual shader metadata'
    $original = Copy-JsonValue $fixture.items
    $original.CustomBuild = @([ordered]@{ Identity='real-source-generator'; Command='generate native source' })
    Assert-Throws { & $assert $original $fixture.items } 'the probe removed an unsupported real source generator'
}

Invoke-FixtureCase 'resolved CL FXC linker and librarian executables must exist and selected tool overrides fail closed' {
    param($root)
    $fixture = New-PerformanceProjectFixture $root
    foreach ($relative in @('toolchain/msvc/bin/Hostx86/x64/cl.exe','toolchain/msvc/bin/Hostx86/x64/link.exe','toolchain/msvc/bin/Hostx86/x64/lib.exe','toolchain/sdk/bin/10.0/x86/fxc.exe')) { Set-FixtureFile $root $relative "actual bytes: $relative" }
    $paths = Get-PerformanceResolvedToolPaths $fixture.properties x64
    Assert-Equal 4 $paths.Count 'all native code producing tools are resolved'
    foreach ($path in $paths.Values) {
        $bytes = [IO.File]::ReadAllBytes($path)
        Remove-Item -LiteralPath $path
        Assert-Throws { Get-PerformanceResolvedToolPaths $fixture.properties x64 } "missing compiler was accepted: $path"
        [IO.File]::WriteAllBytes($path, $bytes)
    }
    foreach ($name in @('CLToolExe','LinkToolExe','LibToolExe','FxCompileToolExe','CLToolPath','LinkToolPath','LibToolPath')) {
        $changed = Copy-JsonValue $fixture.properties
        $changed[$name] = 'unresolved-custom-tool'
        Assert-Throws { Get-PerformanceResolvedToolPaths $changed x64 } "unsupported $name selected a different unattested executable"
    }
}

Invoke-FixtureCase 'real tool identity hashing separates project topology from both compiler profiles and binds tool bytes' {
    param($root)
    Invoke-FixtureGit $root @('init','-q','-b','main')
    Set-FixtureFile $root '.gitignore' ".build/`n"
    $library = New-PerformanceProjectFixture $root
    $benchmark = New-PerformanceProjectFixture $root benchmark
    Set-FixtureFile $root 'src/DxUi.vcxproj' $library.text
    Set-FixtureFile $root 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj' $benchmark.text
    foreach ($path in @('src/Controls/One.cpp','src/Controls/Two.cpp','src/Rendering/CompositeVS.hlsl','src/Rendering/CompositePS.hlsl',
            'Tests/Embedded/Controls/One.cpp','Tests/Embedded/Controls/Two.cpp','include/owned.h')) { Set-FixtureFile $root $path "original explicit native input: $path" }
    Set-FixtureFile $root 'Tools/VisualStudio.psm1' @'
function Get-DxUiVisualStudioInstallation { Join-Path (Split-Path -Parent $PSScriptRoot) 'toolchain/vs' }
Export-ModuleMember -Function Get-DxUiVisualStudioInstallation
'@
    foreach ($path in @('toolchain/vs/MSBuild/Current/Bin/MSBuild.exe','toolchain/vs/MSBuild/Current/Bin/Microsoft.Build.dll','toolchain/vs/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt',
            'toolchain/msvc/bin/Hostx86/x64/cl.exe','toolchain/msvc/bin/Hostx86/x64/link.exe','toolchain/msvc/bin/Hostx86/x64/lib.exe','toolchain/msvc/bin/Hostx86/x64/backend.dll',
            'toolchain/msvc/include/stl.h','toolchain/msvc/include/vector','toolchain/msvc/atlmfc/include/atl.h','toolchain/msvc/atlmfc/lib/x64/atl.lib',
            'toolchain/vs/VC/Auxiliary/VS/include/aux.h','toolchain/vs/VC/Auxiliary/VS/UnitTest/include/test.h','toolchain/vs/VC/Auxiliary/VS/lib/x64/aux.lib','toolchain/vs/VC/Auxiliary/VS/UnitTest/lib/test.lib',
            'toolchain/netfx/Include/um/netfx.h','toolchain/netfx/lib/um/x64/netfx.lib',
            'toolchain/msvc/lib/x64/stl.lib','toolchain/sdk/Include/10.0/um/sdk.h','toolchain/sdk/Include/10.0/shared/shared.h',
            'toolchain/sdk/Include/10.0/winrt/winrt.h','toolchain/sdk/Include/10.0/cppwinrt/winrt/base.h',
            'toolchain/sdk/Include/10.0/ucrt/crt.h','toolchain/sdk/Lib/10.0/um/x64/sdk.lib','toolchain/sdk/Lib/10.0/ucrt/x64/crt.lib',
            'toolchain/sdk/bin/10.0/x86/fxc.exe','toolchain/sdk/bin/10.0/x86/d3dcompiler_47.dll','toolchain/targets/fixture-tasks.dll')) {
        Set-FixtureFile $root $path "original tool input: $path"
    }
    foreach ($path in @('performance.ps1','performance-paired.ps1','build.ps1','vcpkg-install.ps1','Tools/Compare-Performance.ps1','Tools/PerformanceComparison.psm1',
            'Tools/PairedRun.psm1','Tools/BenchmarkGate.psm1','Tools/PerformancePolicy.psm1','Tools/PerformanceAcceptancePolicy.v1.json','Tools/VcpkgTriplet.psm1',
            'Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1','vcpkg.json','vcpkg-tool.json')) { Set-FixtureFile $root $path "fixed fixture harness: $path" }
    Set-FixtureFile $root '.build/vcpkg_installed/x64/vcpkg/status' 'fixture installed dependency'
    Set-FixtureFile $root '.build/vcpkg_installed/x64/vcpkg/info/wil_x64-windows.list' 'x64-windows/include/wil.h'
    Set-FixtureFile $root '.build/vcpkg_installed/x64/x64-windows/include/wil.h' 'fixture installed header'
    Set-FixtureFile $root '.build/vcpkg_installed/x64/x64-windows/share/wil/vcpkg_abi_info.txt' 'fixture ABI'
    Set-FixtureFile $root '.build/x64/Release/runtime.dll' 'fixture output DLL'
    $module = Get-Module PerformancePolicy
    $original = & $module { (Get-Command Get-PerformanceProjectEvaluation).ScriptBlock }
    try {
        # Stub only VS's evaluated output. Production profile validation, source/tool file hashes and identity assembly run.
        & $module {
            param($Library,$Benchmark)
            $script:PerformanceProjectFixtures = @{ library=$Library; benchmark=$Benchmark }
            function script:Get-CimInstance {
                param($ClassName)
                if ($ClassName -eq 'Win32_VideoController') { return [pscustomobject]@{ PNPDeviceID='synthetic GPU'; DriverVersion='fixture'; DriverDate='fixture' } }
                if ($ClassName -eq 'Win32_Processor') { return [pscustomobject]@{ Name='synthetic CPU' } }
                throw "Unexpected fixture CIM query: $ClassName"
            }
            function script:Get-PerformanceProjectEvaluation {
                param($MSBuildPath,$ProjectPath,$Configuration,$Platform,$OutputRoot,$Root,$Role)
                $fixture = $script:PerformanceProjectFixtures[$Role]
                $profile = Get-PerformanceProjectProfile -Root $Root -Role $Role -ExpandedText $fixture.text -Properties $fixture.properties -Items $fixture.items
                $inputs = Get-PerformanceNativeInputClosure -Root $Root -Role $Role -Platform $Platform -Configuration $Configuration -Properties $fixture.properties -Items $fixture.items
                return [ordered]@{ properties=$fixture.properties; profile=$profile; inputs=$inputs; solutionIdentity=(Get-ScopedDigest 'synthetic evaluated solution'); projectSha256=(Get-FileHash -LiteralPath $ProjectPath -Algorithm SHA256).Hash }
            }
        } $library $benchmark
        $originalIdentity = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build
        Assert-Equal 'build-only' $originalIdentity.identityStatus 'complete synthetic evaluation and actual tool bytes are attested without a native build claim'
        $originalFull = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release
        Assert-Equal 'verifiable' $originalFull.identityStatus 'synthetic byte/metadata structure has full tool identity, not qualification evidence'
        $third = Copy-JsonValue $library.items.ClCompile[0]
        $third.Identity = 'Controls\Three.cpp'; $third.FullPath = "$root/src/Controls/Three.cpp"
        Set-FixtureFile $root 'src/Controls/Three.cpp' 'new native source bytes'
        $library.items.ClCompile += $third
        $library.text = $library.text.Replace('<ClCompile Include="Controls\Two.cpp" />', '<ClCompile Include="Controls\Two.cpp" /><ClCompile Include="Controls\Three.cpp" />')
        Set-FixtureFile $root 'src/DxUi.vcxproj' $library.text
        $added = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build
        Assert-Equal 'build-only' $added.identityStatus 'adding a uniform source file retains supported compilation'
        Assert-Equal $originalIdentity.toolchainIdentity $added.toolchainIdentity 'source topology does not become a toolchain mismatch'
        Assert-True ($originalIdentity.resolvedBuildIdentity -cne $added.resolvedBuildIdentity) 'full project bytes and resolved inventory are separately bound'
        $addedFull = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release
        Assert-Equal 'verifiable' $addedFull.identityStatus 'synthetic full identity retains all required fields after source registration'
        Assert-Equal $originalFull.harnessIdentity $addedFull.harnessIdentity 'library project bytes belong per side and cannot change the shared measurement harness'
        Assert-Equal $originalFull.toolchainIdentity $addedFull.toolchainIdentity 'full comparison uses the stable compiler profile'
        foreach ($path in @('src/Controls/One.cpp','Tests/Embedded/Controls/One.cpp','src/Rendering/CompositeVS.hlsl','src/Rendering/CompositePS.hlsl','include/owned.h')) {
            $full = Join-Path $root $path
            $bytes = [IO.File]::ReadAllText($full)
            $time = [IO.File]::GetLastWriteTimeUtc($full)
            Set-FixtureFile $root $path ($bytes + ' changed without advancing time')
            [IO.File]::SetLastWriteTimeUtc($full, $time)
            $changed = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build
            Assert-Equal 'build-only' $changed.identityStatus 'ordinary source byte changes remain supported'
            Assert-True ($added.resolvedBuildIdentity -cne $changed.resolvedBuildIdentity) "resolved native bytes are attested per side: $path"
            Assert-Equal $added.toolchainIdentity $changed.toolchainIdentity 'source bytes cannot falsely change the stable compiler profile'
            Set-FixtureFile $root $path $bytes
        }
        foreach ($role in @('library','benchmark')) {
            $fixture = if ($role -eq 'library') { $library } else { $benchmark }
            foreach ($item in $fixture.items.ClCompile) { $item.AdditionalOptions += ' /changed' }
            $flags = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build
            Assert-Equal 'build-only' $flags.identityStatus 'uniform option changes remain structurally supported'
            Assert-True ($added.toolchainIdentity -cne $flags.toolchainIdentity) "$role compiler flags cannot hide in per-side source identity"
            foreach ($item in $fixture.items.ClCompile) { $item.AdditionalOptions = $item.AdditionalOptions.Replace(' /changed','') }
        }
        foreach ($path in @('toolchain/msvc/bin/Hostx86/x64/cl.exe','toolchain/msvc/bin/Hostx86/x64/backend.dll','toolchain/msvc/bin/Hostx86/x64/link.exe',
                'toolchain/msvc/bin/Hostx86/x64/lib.exe','toolchain/sdk/bin/10.0/x86/fxc.exe','toolchain/sdk/bin/10.0/x86/d3dcompiler_47.dll','toolchain/targets/fixture-tasks.dll',
                'toolchain/vs/MSBuild/Current/Bin/Microsoft.Build.dll','toolchain/msvc/include/vector','toolchain/msvc/atlmfc/include/atl.h',
                'toolchain/sdk/Include/10.0/winrt/winrt.h','toolchain/sdk/Include/10.0/cppwinrt/winrt/base.h','toolchain/netfx/Include/um/netfx.h',
                'toolchain/vs/VC/Auxiliary/VS/UnitTest/include/test.h','toolchain/vs/VC/Auxiliary/VS/lib/x64/aux.lib')) {
            $full = Join-Path $root $path
            $bytes = [IO.File]::ReadAllText($full)
            Set-FixtureFile $root $path ($bytes + ' changed')
            $changed = Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build
            Assert-Equal 'build-only' $changed.identityStatus 'a byte change remains measurable as a different toolchain'
            Assert-True ($added.toolchainIdentity -cne $changed.toolchainIdentity) "actual tool bytes must be bound: $path"
            Set-FixtureFile $root $path $bytes
        }
        $library.items.ClCompile[0].Optimization = 'Disabled'
        Assert-Equal 'identity-unverifiable' (Get-PerformanceToolIdentities -Root $root -Platform x64 -Configuration Release -IdentityScope Build).identityStatus 'a per-file option override cannot publish incomplete tool evidence'
    } finally {
        & $module { param($Original) Set-Item -LiteralPath 'Function:script:Get-PerformanceProjectEvaluation' -Value $Original; Remove-Variable -Name PerformanceProjectFixtures -Scope Script; Remove-Item -LiteralPath 'Function:script:Get-CimInstance' } $original
    }
}

function Get-FixtureNativeClosure([Collections.IDictionary] $Fixture) {
    return & (Get-Module PerformancePolicy) {
        param($Fixture)
        Get-PerformanceNativeInputClosure -Root $Fixture.root -Role $Fixture.role -Platform x64 -Configuration Release -Properties $Fixture.properties -Items $Fixture.items
    } $Fixture
}

Invoke-FixtureCase 'native and incremental root junctions cannot redirect attested inputs or cached build outputs' {
    param($root)
    Invoke-FixtureGit $root @('init','-q','-b','main')
    Set-FixtureFile $root '.gitignore' ".build/`n"
    $fixture = New-PerformanceProjectFixture $root
    foreach ($path in @('src/Controls/One.cpp','src/Controls/Two.cpp','src/Rendering/CompositeVS.hlsl','src/Rendering/CompositePS.hlsl','include/owned.h')) { Set-FixtureFile $root $path '// owned native bytes' }
    Get-FixtureNativeClosure $fixture | Out-Null
    $prefix = [IO.Path]::GetFullPath($root).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $linkType = if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) { 'Junction' } else { 'SymbolicLink' }
    foreach ($relative in @('include','.build/Intermediate/x64/Release/DxUi','.build/Intermediate/x64/Release')) {
        $link = [IO.Path]::GetFullPath((Join-Path $root $relative))
        $target = [IO.Path]::GetFullPath((Join-Path $root ('.build/reparse-' + [guid]::NewGuid().ToString('N'))))
        foreach ($path in @($link,$target)) { if (-not $path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Reparse fixture path escaped its owned root.' } }
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        $restore = Test-Path -LiteralPath $link
        if ($restore) { Move-Item -LiteralPath $link -Destination $target }
        else { New-Item -ItemType Directory -Path $target -Force | Out-Null }
        New-Item -ItemType Directory -Path (Split-Path -Parent $link) -Force | Out-Null
        try {
            New-Item -ItemType $linkType -Path $link -Target $target | Out-Null
            if ($relative -eq '.build/Intermediate/x64/Release') {
                Assert-Throws { & (Get-Module PerformancePolicy) { param($Root) Get-PerformanceIntermediateIdentity $Root x64 Release } $root } 'a redirected incremental profile root was accepted'
            } else {
                Assert-Throws { Get-FixtureNativeClosure $fixture } "a redirected native input/cache root was accepted: $relative"
            }
        } finally {
            if (Test-Path -LiteralPath $link) { Remove-Item -LiteralPath $link -Force }
            if ($restore) { Move-Item -LiteralPath $target -Destination $link }
        }
    }
    Get-FixtureNativeClosure $fixture | Out-Null
}

Invoke-FixtureCase 'resolved native inputs require owned bytes and reject ignored external forced response and cache-shadow inputs' {
    param($root)
    Invoke-FixtureGit $root @('init','-q','-b','main')
    Set-FixtureFile $root '.gitignore' ".build/`ninclude/ignored.h`n"
    $fixture = New-PerformanceProjectFixture $root
    foreach ($path in @('src/Controls/One.cpp','src/Controls/Two.cpp','src/Rendering/CompositeVS.hlsl','src/Rendering/CompositePS.hlsl','include/owned.h')) { Set-FixtureFile $root $path '// owned native bytes' }
    $before = Get-FixtureNativeClosure $fixture
    $good = Copy-JsonValue $fixture
    $good.items.ClCompile[0].ForcedIncludeFiles = "$root/include/owned.h"
    $good.properties.ReferencePath = "$root/toolchain/msvc/atlmfc/lib/x64;$root/toolchain/msvc/lib/x64"
    Get-FixtureNativeClosure $good | Out-Null
    Set-FixtureFile $root 'src/Controls/One.cpp' '#include "../../include/owned.h"'
    Get-FixtureNativeClosure $fixture | Out-Null
    foreach ($text in @('#include "../../../outside.h"','#include "C:/outside.h"','#include UNKNOWN_HEADER', '#include "cached.obj"', '#pragma comment(lib, "C:/outside.lib")',
            '#if __has_include("C:/outside.h")','#using "C:/outside.dll"','#embed "C:/outside.bin"')) {
        Set-FixtureFile $root 'src/Controls/One.cpp' $text
        Assert-Throws { Get-FixtureNativeClosure $fixture } "unsupported source-selected input was accepted: $text"
    }
    Set-FixtureFile $root 'src/Controls/One.cpp' '// owned native bytes'
    foreach ($case in @(
        @('FullPath',"$root/outside.cpp"), @('AdditionalIncludeDirectories',"$root/ignored"), @('ExternalIncludeDirectories',"$root/ignored"),
        @('ForcedIncludeFiles',"$root/.build/foreign.h"), @('ForcedUsingFiles',"$root/.build/foreign.dll"),
        @('AdditionalOptions','@C:/foreign.rsp'), @('AdditionalOptions','/IC:/foreign'), @('AdditionalOptions','/FIforeign.h'),
        @('AdditionalOptions','/external:I C:/foreign'), @('AdditionalOptions','/TpC:/foreign.cpp'), @('AdditionalOptions','C:/foreign.obj'), @('ObjectFileName',"$root/.build/foreign-objects/"),
        @('PrecompiledHeader','Use'), @('BuildStlModules','true'))) {
        $bad = Copy-JsonValue $fixture
        $bad.items.ClCompile[0][$case[0]] = $case[1]
        Assert-Throws { Get-FixtureNativeClosure $bad } "unsupported compiler input was accepted: $($case[0])=$($case[1])"
    }
    foreach ($case in @(@('LibraryPath',"$root/ignored-libs"), @('ReferencePath','C:/foreign-references'), @('LibAdditionalOptions','/LIBPATH:C:/foreign'))) {
        $bad = Copy-JsonValue $fixture
        $bad.properties[$case[0]] = $case[1]
        Assert-Throws { Get-FixtureNativeClosure $bad } "unsupported global native input was accepted: $($case[0])"
    }
    foreach ($case in @(@('AdditionalLibraryDirectories','C:/foreign'), @('AdditionalDependencies','C:/foreign.lib'), @('AdditionalDependencies','unknown.lib'), @('AdditionalOptions','/DEF:C:/foreign.def'))) {
        $bad = Copy-JsonValue $fixture
        $bad.items.Link[0][$case[0]] = $case[1]
        Assert-Throws { Get-FixtureNativeClosure $bad } "unsupported linker input was accepted: $($case[0])"
    }
    $old = [Environment]::GetEnvironmentVariable('CL')
    try {
        [Environment]::SetEnvironmentVariable('CL','@C:/implicit.rsp')
        Assert-Throws { Get-FixtureNativeClosure $fixture } 'an implicit CL response path was accepted'
    } finally { [Environment]::SetEnvironmentVariable('CL',$old) }
    Set-FixtureFile $root 'include/ignored.h' 'ignored header'
    Assert-Throws { Get-FixtureNativeClosure $fixture } 'ignored bytes under an active include root were accepted'
    Remove-Item -LiteralPath (Join-Path $root 'include/ignored.h')
    foreach ($name in @('vector','alternate.txt','foreign.h','DxUiCompositeVS.hpp','nested/foreign.inc')) {
        $path = ".build/Intermediate/x64/Release/DxUi/$name"
        Set-FixtureFile $root $path 'foreign shadow input'
        Assert-Throws { Get-FixtureNativeClosure $fixture } "ignored intermediate shadow input was accepted: $name"
        Remove-Item -LiteralPath (Join-Path $root $path)
    }
    foreach ($name in @('DxUiCompositeVS.h','DxUiCompositePS.h','DxUiCompositeVS.cso','DxUiCompositePS.cso','Control.obj','DxUi.tlog/CL.read.1.tlog','DxUi.ilk')) {
        Set-FixtureFile $root ".build/Intermediate/x64/Release/DxUi/$name" 'supported derived output'
    }
    Assert-Equal $before.identity (Get-FixtureNativeClosure $fixture).identity 'disposable supported derived outputs do not become source bytes or final SkipBuild requirements'
    $missing = Join-Path $root 'src/Controls/One.cpp'
    Remove-Item -LiteralPath $missing
    Assert-Throws { Get-FixtureNativeClosure $fixture } 'a missing resolved source file was attested'
}

Invoke-FixtureCase 'project evaluation uses the attested checkout context and restores the caller after failure' {
    param($root)
    $module = Get-Module PerformancePolicy
    $original = & $module { (Get-Command Get-PerformanceProjectEvaluationCore).ScriptBlock }
    $caller = (Get-Location).Path
    try {
        & $module {
            function script:Get-PerformanceProjectEvaluationCore {
                param($MSBuildPath,$ProjectPath,$Configuration,$Platform,$OutputRoot,$Root,$Role)
                if ($Role -eq 'fail') { throw 'injected evaluation failure' }
                return [ordered]@{ startupDirectory=(Get-Location).Path; root=$Root }
            }
        }
        $result = & $module { param($Root) Get-PerformanceProjectEvaluation -Root $Root -Role library } $root
        Assert-Equal ([IO.Path]::GetFullPath($root)) ([IO.Path]::GetFullPath($result.startupDirectory)) 'MSBuild evaluates the baseline in its own checkout'
        Assert-Equal $caller (Get-Location).Path 'successful evaluation restores the caller'
        Assert-Throws { & $module { param($Root) Get-PerformanceProjectEvaluation -Root $Root -Role fail } $root } 'evaluation failure was not injected'
        Assert-Equal $caller (Get-Location).Path 'failed evaluation restores the caller'
    } finally {
        & $module { param($Original) Set-Item -LiteralPath 'Function:script:Get-PerformanceProjectEvaluationCore' -Value $Original } $original
    }
}

Invoke-FixtureCase 'a module reload cannot forget the byte identity of its already loaded MSBuild parser' {
    param($root)
    $key = 'DxUi.Performance.MSBuildAssembly.v2'
    $prior = [AppDomain]::CurrentDomain.GetData($key)
    try {
        Set-FixtureFile $root 'engine/Microsoft.Build.dll' 'previously loaded parser bytes'
        $path = Join-Path $root 'engine/Microsoft.Build.dll'
        [AppDomain]::CurrentDomain.SetData($key, @{ path=$path; sha=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash; assembly=$null })
        Import-Module (Join-Path $PSScriptRoot '../PerformancePolicy.psm1') -Force
        Set-FixtureFile $root 'engine/Microsoft.Build.dll' 'replacement parser bytes'
        $message = ''
        try { & (Get-Module PerformancePolicy) { param($Root) Get-PerformanceSolutionConfiguration (Join-Path $Root 'engine/MSBuild.exe') (Join-Path $Root 'DxUi.sln') Release x64 } $root }
        catch { $message = $_.Exception.Message }
        Assert-True $message.Contains('loaded MSBuild parser changed') 'replacement bytes were incorrectly treated as the previously loaded parser after module reload'
    } finally { [AppDomain]::CurrentDomain.SetData($key, $prior) }
}

Invoke-FixtureCase 'solution globals bind the native solution XML and reject missing duplicate or fallback build mappings' {
    param($root)
    $matrix = @('Debug|x64','Release|x64','ASan Debug|x64','Debug|ARM64','Release|ARM64','ASan Debug|ARM64')
    $projects = [ordered]@{ '{F6272096-E42A-56E7-9CCB-A8E08093E2BC}'='src\DxUi.vcxproj'; '{8B4FAB6F-83CD-48EF-A16E-21CD97073F92}'='Tests\Embedded\DxUi.EmbeddedTests.vcxproj' }
    $text = "Microsoft Visual Studio Solution File, Format Version 12.00`n"
    foreach ($guid in $projects.Keys) { $text += "Project(`"{BC8A1FFA-BEE3-4634-8014-F334798102B3}`") = `"fixture`", `"$($projects[$guid])`", `"$guid`"`nEndProject`n" }
    $text += "Global`nGlobalSection(SolutionConfigurationPlatforms) = preSolution`n"
    foreach ($name in $matrix) { $text += "  $name = $name`n" }
    $text += "EndGlobalSection`nGlobalSection(ProjectConfigurationPlatforms) = postSolution`n"
    foreach ($guid in $projects.Keys) { foreach ($name in $matrix) { foreach ($kind in @('ActiveCfg','Build.0')) { $text += "  $guid.$name.$kind = $name`n" } } }
    $text += "EndGlobalSection`nEndGlobal`n"
    Set-FixtureFile $root 'DxUi.sln' $text
    $document = [xml]'<SolutionConfiguration />'
    foreach ($guid in $projects.Keys) {
        $node = $document.CreateElement('ProjectConfiguration')
        $node.SetAttribute('Project', $guid)
        $node.SetAttribute('AbsolutePath', [IO.Path]::GetFullPath((Join-Path $root $projects[$guid])))
        $node.SetAttribute('BuildProjectInSolution', 'True')
        $node.InnerText = 'Release|x64'
        [void]$document.DocumentElement.AppendChild($node)
    }
    $module = Get-Module PerformancePolicy
    $original = & $module { (Get-Command Get-PerformanceSolutionConfiguration).ScriptBlock }
    try {
        & $module {
            param($Xml)
            $script:PerformanceSolutionFixture = $Xml
            function script:Get-PerformanceSolutionConfiguration { param($MSBuildPath,$SolutionPath,$Configuration,$Platform) return $script:PerformanceSolutionFixture }
        } $document.OuterXml
        $globals = & $module { param($Root) Get-PerformanceSolutionGlobals fake-msbuild $Root Release x64 } $root
        Assert-Equal 'true' $globals.properties.BuildingSolutionFile 'evaluation uses the solution build path'
        Assert-Equal $document.OuterXml $globals.properties.CurrentSolutionConfigurationContents 'exact native generated XML is passed through'
        Assert-Equal ([IO.Path]::GetFullPath((Join-Path $root 'DxUi.sln'))) $globals.properties.SolutionPath 'solution paths name the measured tree'
        $arguments = @(& $module { param($Properties) ConvertTo-PerformanceMSBuildProperties $Properties } $globals.properties)
        $xmlArgument = @($arguments | Where-Object { $_.StartsWith('/p:CurrentSolutionConfigurationContents=') })[0]
        Assert-True ($xmlArgument.Contains('%22') -and -not $xmlArgument.Contains('"')) 'native XML quotes cannot be removed by property argument parsing'
        $selected = '{F6272096-E42A-56E7-9CCB-A8E08093E2BC}.Release|x64'
        $line = "  $selected.Build.0 = Release|x64`n"
        foreach ($changed in @($text.Replace($line,''), $text.Replace($line,$line+$line),
                $text.Replace("$selected.ActiveCfg = Release|x64", "$selected.ActiveCfg = Debug|x64"),
                $text.Replace("$selected.Build.0 = Release|x64", "$selected.Build.0 = Debug|x64"))) {
            Set-FixtureFile $root 'DxUi.sln' $changed
            Assert-Throws { & $module { param($Root) Get-PerformanceSolutionGlobals fake-msbuild $Root Release x64 } $root } 'invalid solution mapping was accepted'
        }
        Set-FixtureFile $root 'DxUi.sln' $text
        foreach ($mutation in @('disabled','missing','duplicate','redirected','configuration')) {
            $changed = [xml]$document.OuterXml
            $node = $changed.DocumentElement.FirstChild
            switch ($mutation) {
                disabled { $node.SetAttribute('BuildProjectInSolution','False') }
                missing { [void]$changed.DocumentElement.RemoveChild($node) }
                duplicate { [void]$changed.DocumentElement.AppendChild($node.CloneNode($true)) }
                redirected { $node.SetAttribute('AbsolutePath', (Join-Path $root 'other-library.vcxproj')) }
                configuration { $node.InnerText = 'Debug|x64' }
            }
            & $module { param($Xml) $script:PerformanceSolutionFixture=$Xml } $changed.OuterXml
            Assert-Throws { & $module { param($Root) Get-PerformanceSolutionGlobals fake-msbuild $Root Release x64 } $root } "native solution XML $mutation was accepted"
        }
    } finally {
        & $module { param($Original) Set-Item -LiteralPath 'Function:script:Get-PerformanceSolutionConfiguration' -Value $Original; Remove-Variable -Name PerformanceSolutionFixture -Scope Script } $original
    }
}

function Invoke-PerformanceBuildFixture([string] $Root, [scriptblock] $Body) {
    Invoke-FixtureGit $Root @('init','-q','-b','main')
    Set-FixtureFile $Root '.gitignore' ".build/`n"
    Set-FixtureFile $Root 'src/Control.cpp' 'original compiled source'
    Set-FixtureFile $Root '.build/toolchain/compiler.bin' 'original resolved compiler'
    Set-FixtureFile $Root '.build/toolchain/fxc.bin' 'original resolved shader compiler'
    Set-FixtureFile $Root '.build/toolchain/linker.bin' 'original resolved linker'
    Set-FixtureFile $Root '.build/toolchain/archiver.bin' 'original resolved static library archiver'
    Set-FixtureFile $Root '.build/toolchain/resolved-project.json' '{"library":"resolved original project","benchmark":"resolved original project"}'
    Set-FixtureFile $Root 'src/DxUi.vcxproj' '<Project><ClCompile Include="Control.cpp" /></Project>'
    Set-FixtureFile $Root 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj' '<Project><ClCompile Include="Benchmark.cpp" /></Project>'
    Set-FixtureFile $Root 'Directory.Build.props' '<Project><PropertyGroup><Option>original</Option></PropertyGroup></Project>'
    Set-FixtureFile $Root 'Build/Imported.targets' '<Project><Target Name="Original" /></Project>'
    foreach ($entry in @(
        @('.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h', 'original WIL header'),
        @('.build/vcpkg_installed/x64/x64-windows/share/wil/vcpkg_abi_info.txt', 'original dependency ABI'),
        @('.build/vcpkg_installed/x64/x64-windows/lib/wil.lib', 'original installed library'),
        @('.build/vcpkg_installed/x64/vcpkg/status', 'original package status'),
        @('.build/vcpkg_installed/x64/vcpkg/info/wil_fixture_x64-windows.list', 'original package inventory'),
        @('.build/x64/Release/DxUi.EmbeddedTests.exe', 'original benchmark executable'),
        @('.build/x64/Release/DxUi.lib', 'original static library'),
        @('.build/x64/Release/runtime.dll', 'original runtime DLL'))) {
        Set-FixtureFile $Root $entry[0] $entry[1]
    }
    foreach ($path in @('DxUi/Control.obj','DxUi/DxUi.tlog/CL.read.tlog','DxUi/DxUiCompositeVS.h','DxUi/DxUiCompositePS.h',
            'DxUi/DxUiCompositeVS.cso','DxUi/DxUiCompositePS.cso','DxUi.EmbeddedTests/Benchmark.obj','DxUi.EmbeddedTests/Benchmark.tlog/link.read.tlog',
            'DxUi.FoundationTests/Foundation.obj','DxUi.ControlTests/other.cache')) { Set-FixtureFile $Root ".build/Intermediate/x64/Release/$path" "original intermediate: $path" }
    Invoke-FixtureGit $Root @('add','-A')
    Invoke-FixtureGit $Root @('commit','-q','-m','benchmark fixture')
    $module = Get-Module PerformancePolicy
    $original = & $module { (Get-Command Get-PerformanceToolIdentities).ScriptBlock }
    try {
        # Replace only VS/SDK discovery. Source, installed dependencies, output artifacts and receipts use real byte hashes.
        & $module {
            function script:Get-PerformanceToolIdentities {
                param($Root, $Platform, $Configuration, $IdentityScope = 'Full')
                $compiler = @('compiler.bin','fxc.bin','linker.bin','archiver.bin' | ForEach-Object { (Get-FileHash -LiteralPath (Join-Path $Root ".build/toolchain/$_") -Algorithm SHA256).Hash }) -join "`n"
                $options = [Environment]::GetEnvironmentVariable('CL')
                $toolchain = Get-ScopedDigest ($compiler + "`n" + $options)
                $resolved = (Get-FileHash -LiteralPath (Join-Path $Root '.build/toolchain/resolved-project.json') -Algorithm SHA256).Hash
                return [ordered]@{ buildIdentityVersion=2; resolvedBuildIdentity=$resolved; identityStatus=$(if ($IdentityScope -eq 'Build') { 'build-only' } else { 'verifiable' });
                    identityScope=$(if ($IdentityScope -eq 'Build') { 'build-only' } else { 'full' }); toolchainIdentity=$toolchain }
            }
        }
        & $Body $Root
    } finally {
        & $module { param($Original) Set-Item -LiteralPath 'Function:script:Get-PerformanceToolIdentities' -Value $Original } $original
    }
}

Invoke-FixtureCase 'benchmark reuse rejects stale source, toolchain, installed dependency and output bytes' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        foreach ($path in @('src/Control.cpp', 'src/DxUi.vcxproj', 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj', 'Directory.Build.props', 'Build/Imported.targets',
                '.build/toolchain/compiler.bin', '.build/toolchain/fxc.bin', '.build/toolchain/linker.bin', '.build/toolchain/archiver.bin', '.build/toolchain/resolved-project.json',
                '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h',
                '.build/vcpkg_installed/x64/x64-windows/lib/wil.lib',
                '.build/vcpkg_installed/x64/vcpkg/status',
                '.build/vcpkg_installed/x64/vcpkg/info/wil_fixture_x64-windows.list',
                '.build/vcpkg_installed/x64/x64-windows/share/wil/vcpkg_abi_info.txt',
                '.build/x64/Release/DxUi.EmbeddedTests.exe', '.build/x64/Release/DxUi.lib', '.build/x64/Release/runtime.dll')) {
            $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
            Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            $full = Join-Path $fixture $path
            $original = [IO.File]::ReadAllText($full)
            $exeHash = (Get-FileHash -LiteralPath (Join-Path $fixture '.build/x64/Release/DxUi.EmbeddedTests.exe') -Algorithm SHA256).Hash
            Set-FixtureFile $fixture $path ($original + ' mutated after build')
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } "stale benchmark build accepted: $path"
            if ($path -ne '.build/x64/Release/DxUi.EmbeddedTests.exe') {
                Assert-Equal $exeHash (Get-FileHash -LiteralPath (Join-Path $fixture '.build/x64/Release/DxUi.EmbeddedTests.exe') -Algorithm SHA256).Hash 'unchanged executable cannot establish a current source/tool/dependency build'
            }
            Set-FixtureFile $fixture $path $original
            Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        }
    }
}

Invoke-FixtureCase 'a failed build attempt revokes a previous successful benchmark attestation' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        # Compilation fails before Complete can publish. All source and old executable bytes remain identical.
        Assert-Throws { throw 'injected compiler failure' } 'the simulated compiler failure was not injected'
        Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'a failed rebuild reused the predecessor pass'
    }
}

Invoke-FixtureCase 'failed native build markers permit a guarded retry only in each canonical project tracking directory' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixtureRoot)
        $fixtures = @{ library=(New-PerformanceProjectFixture $fixtureRoot); benchmark=(New-PerformanceProjectFixture $fixtureRoot benchmark) }
        foreach ($path in @('src/Controls/One.cpp','src/Controls/Two.cpp','src/Rendering/CompositeVS.hlsl','src/Rendering/CompositePS.hlsl',
                'Tests/Embedded/Controls/One.cpp','Tests/Embedded/Controls/Two.cpp','include/owned.h')) { Set-FixtureFile $fixtureRoot $path '// synthetic native input bytes' }
        $module = Get-Module PerformancePolicy
        $original = & $module { (Get-Command Get-PerformanceToolIdentities).ScriptBlock }
        try {
            # Keep the existing synthetic VS discovery, but run the production native closure and build/receipt guards.
            & $module {
                param($Fixtures,$Original)
                $script:NativeRetryFixtures = $Fixtures
                $script:NativeRetryToolDiscovery = $Original
                function script:Get-PerformanceToolIdentities {
                    param($Root,$Platform,$Configuration,$IdentityScope='Full')
                    $identity = & $script:NativeRetryToolDiscovery @PSBoundParameters
                    foreach ($role in @('library','benchmark')) {
                        $fixture = $script:NativeRetryFixtures[$role]
                        $native = Get-PerformanceNativeInputClosure -Root $Root -Role $role -Platform $Platform -Configuration $Configuration -Properties $fixture.properties -Items $fixture.items
                        $identity['resolvedBuildIdentity'] = Get-ScopedDigest ($identity['resolvedBuildIdentity'] + "`n" + $native.identity)
                    }
                    return $identity
                }
            } $fixtures $original
            $before = Start-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release
            Complete-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release -Before $before
            $failed = Start-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release
            foreach ($project in @('DxUi','DxUi.EmbeddedTests')) {
                Set-FixtureFile $fixtureRoot ".build/Intermediate/x64/Release/$project/$project.tlog/unsuccessfulbuild" ''
            }
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release } 'the failed attempt cannot leave a passing final receipt'
            $retry = Start-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release
            Assert-True $retry.rebuildRequired 'the failed build retries with Rebuild instead of reusing prior outputs'
            Assert-Equal $failed.buildInputIdentity $retry.buildInputIdentity 'canonical failure status markers are not source inputs'
            foreach ($project in @('DxUi','DxUi.EmbeddedTests')) {
                foreach ($name in @('unsuccessfulbuild','Other.tlog/unsuccessfulbuild',"$project.tlog/nested/unsuccessfulbuild",'vector',"$project.tlog/vector")) {
                    $path = ".build/Intermediate/x64/Release/$project/$name"
                    Set-FixtureFile $fixtureRoot $path 'foreign extensionless input'
                    Assert-Throws { Start-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release } "noncanonical marker or include shadow was accepted: $project/$name"
                    Remove-Item -LiteralPath (Join-Path $fixtureRoot $path)
                }
            }
            $retry = Start-PerformanceBuildAttestation -Root $fixtureRoot -Platform x64 -Configuration Release
            Assert-True $retry.rebuildRequired 'removing foreign shadows restores the guarded retry path'
        } finally {
            & $module { param($Original) Set-Item -LiteralPath 'Function:script:Get-PerformanceToolIdentities' -Value $Original; Remove-Variable -Name NativeRetryFixtures,NativeRetryToolDiscovery -Scope Script } $original
        }
    }
}

Invoke-FixtureCase 'incremental builds require an exact current v2 receipt including inputs whose timestamps did not change' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True $before.rebuildRequired 'an unattested default build must fully rebuild'
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        $matching = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True (-not $matching.rebuildRequired) 'exact prior v2 inputs and outputs permit an incremental build'
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $matching
        foreach ($relative in @('src/Control.cpp','src/DxUi.vcxproj','.build/toolchain/compiler.bin','.build/toolchain/resolved-project.json',
                '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h','.build/x64/Release/DxUi.lib')) {
            $path = Join-Path $fixture $relative
            $bytes = [IO.File]::ReadAllText($path)
            $stamp = [IO.File]::GetLastWriteTimeUtc($path)
            Set-FixtureFile $fixture $relative ($bytes + ' changed while preserving mtime')
            [IO.File]::SetLastWriteTimeUtc($path, $stamp)
            $changed = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            Assert-True $changed.rebuildRequired "$relative cannot inherit stale objects through an incremental timestamp check"
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'the prior receipt was not revoked'
            Set-FixtureFile $fixture $relative $bytes
            $restored = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            Assert-True $restored.rebuildRequired 'no receipt remains after an attempted build'
            Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $restored
        }
    }
}

Invoke-FixtureCase 'default build entrypoint rebuilds stale inputs and preserves explicit rebuild and consumer output behavior' {
    param($root)
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../../build.ps1'), [ref]$tokens, [ref]$errors)
    Assert-Equal 0 $errors.Count 'the production build entrypoint parses'
    $initial = $ast.Find({ param($Node) $Node -is [Management.Automation.Language.AssignmentStatementAst] -and $Node.Left.Extent.Text -ceq '$target' }, $true)
    $guard = $ast.Find({ param($Node) $Node -is [Management.Automation.Language.IfStatementAst] -and $Node.Extent.Text.Contains('Start-PerformanceBuildAttestation') }, $true)
    $arguments = $ast.Find({ param($Node) $Node -is [Management.Automation.Language.AssignmentStatementAst] -and $Node.Left.Extent.Text -ceq '$arguments' }, $true)
    # Execute the actual pre-MSBuild entrypoint stage with real v2 attestation logic. No native build is invoked;
    # only its import is suppressed because the test already imported the production module and supplies a VS fixture.
    $stage = [scriptblock]::Create('param($PSScriptRoot,$Platform,$Configuration,$attestPerformanceBuild,$Rebuild)' + "`n" +
        "function Import-Module { param(`$Name, [switch]`$Force) }`n" + $initial.Extent.Text + "`n" + $guard.Extent.Text + "`n" +
        '$log="fixture.log"; $output=(Join-Path $PSScriptRoot ''.build'')' + "`n" + $arguments.Extent.Text + "`n" +
        '[ordered]@{ arguments=$arguments; state=$(if ($attestPerformanceBuild) { $performanceBuildBefore } else { $null }) }')
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        foreach ($case in @(@($true,$false,'/t:Rebuild'), @($true,$false,'/t:Build'), @($true,$true,'/t:Rebuild'), @($false,$false,'/t:Build'))) {
            $result = & $stage $fixture x64 Release $case[0] $case[1] 6>$null
            Assert-Contains $result.arguments $case[2] 'the actual native invocation receives the required build target'
            if ($case[0]) { Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $result.state }
        }
        $path = Join-Path $fixture '.build/toolchain/compiler.bin'
        [IO.File]::WriteAllText($path, 'changed compiler bytes')
        $result = & $stage $fixture x64 Release $true $false 6>$null
        Assert-Contains $result.arguments '/t:Rebuild' 'a stale receipt forces the entrypoint to recompile rather than stamp existing object files'
    }
}

Invoke-FixtureCase 'incremental reuse binds the full intermediate inventory while SkipBuild remains final-output only' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $module = Get-Module PerformancePolicy
        $paths = & $module {
            param($Root)
            @((Get-PerformanceBuildAttestationPath $Root x64 Release), (Get-PerformanceIncrementalAttestationPath $Root x64 Release))
        } $fixture
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        Remove-Item -LiteralPath $paths[1]
        Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True $before.rebuildRequired 'a valid final-only v2 receipt cannot authorize incremental Build'
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        foreach ($relative in @('DxUi/Control.obj','DxUi/DxUi.tlog/CL.read.tlog','DxUi/DxUiCompositeVS.h','DxUi/DxUiCompositePS.cso',
                'DxUi.EmbeddedTests/Benchmark.obj','DxUi.FoundationTests/Foundation.obj','DxUi.ControlTests/other.cache')) {
            $path = Join-Path $fixture ".build/Intermediate/x64/Release/$relative"
            $bytes = [IO.File]::ReadAllText($path)
            $time = [IO.File]::GetLastWriteTimeUtc($path)
            [IO.File]::WriteAllText($path, $bytes + ' foreign cache bytes')
            [IO.File]::SetLastWriteTimeUtc($path, $time)
            Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            Assert-True $before.rebuildRequired "changed cache bytes were reused despite an unchanged timestamp: $relative"
            foreach ($receipt in $paths) { Assert-True (-not (Test-Path -LiteralPath $receipt)) 'both receipts are revoked before rebuilding' }
            [IO.File]::WriteAllText($path, $bytes)
            Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        }
        $extra = Join-Path $fixture '.build/Intermediate/x64/Release/DxUi.ControlTests/added-output.txt'
        [IO.File]::WriteAllText($extra, 'unseen intermediate')
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True $before.rebuildRequired 'adding any profile intermediate invalidates incremental reuse'
        Remove-Item -LiteralPath $extra
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        $path = Join-Path $fixture '.build/Intermediate/x64/Release/DxUi.FoundationTests/Foundation.obj'
        $renamed = Join-Path (Split-Path -Parent $path) 'Renamed.obj'
        Move-Item -LiteralPath $path -Destination $renamed
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True $before.rebuildRequired 'path inventory is bound independently of file contents'
        Move-Item -LiteralPath $renamed -Destination $path
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        $shader = Join-Path $fixture '.build/Intermediate/x64/Release/DxUi/DxUiCompositeVS.h'
        $bytes = [IO.File]::ReadAllText($shader)
        Remove-Item -LiteralPath $shader
        Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True $before.rebuildRequired 'missing required shader intermediates force Rebuild'
        Assert-Throws { Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before } 'an incomplete build cache published a successful receipt'
        foreach ($receipt in $paths) { Assert-True (-not (Test-Path -LiteralPath $receipt)) 'incomplete publication revokes both receipts' }
        [IO.File]::WriteAllText($shader,$bytes)
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Assert-True (-not $before.rebuildRequired) 'the exact full closure permits incremental Build again'
    }
}

Invoke-FixtureCase 'changed implicit compiler options invalidate a reused or ongoing benchmark build' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $priorOptions = [Environment]::GetEnvironmentVariable('CL')
        try {
            [Environment]::SetEnvironmentVariable('CL', '/DPERFORMANCE_BUILD_FIXTURE=1')
            $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
            Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            [Environment]::SetEnvironmentVariable('CL', '/DPERFORMANCE_BUILD_FIXTURE=2')
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'changed CL options reused a prior build'
            $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            [Environment]::SetEnvironmentVariable('CL', '/DPERFORMANCE_BUILD_FIXTURE=3')
            Assert-Throws { Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before } 'mid-build CL changes published an attestation'
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'failed implicit compiler identity left a passing receipt'
        } finally { [Environment]::SetEnvironmentVariable('CL', $priorOptions) }
    }
}

Invoke-FixtureCase 'mid-build source, dependency or compiler mutation cannot publish a benchmark build receipt' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        foreach ($path in @('src/Control.cpp', 'src/DxUi.vcxproj', 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj', 'Directory.Build.props', 'Build/Imported.targets',
                '.build/toolchain/resolved-project.json', '.build/toolchain/compiler.bin', '.build/toolchain/fxc.bin', '.build/toolchain/linker.bin', '.build/toolchain/archiver.bin',
                '.build/vcpkg_installed/x64/x64-windows/include/wil/resource.h')) {
            $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
            $original = [IO.File]::ReadAllText((Join-Path $fixture $path))
            Set-FixtureFile $fixture $path ($original + ' mutated during build')
            Assert-Throws { Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before } "mid-build mutation was accepted: $path"
            Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'failed provenance published a build receipt'
            Set-FixtureFile $fixture $path $original
        }
    }
}

Invoke-FixtureCase 'a structurally valid v1 receipt cannot authorize v2 benchmark build reuse' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $before = Start-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        Complete-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release -Before $before
        Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release
        $module = Get-Module PerformancePolicy
        & $module {
            param($Root)
            $state = Get-PerformanceBuildState -Root $Root -Platform x64 -Configuration Release
            $artifact = Get-ScopedArtifactIdentity $Root x64 Release
            $v1 = Get-ScopedDigest (@('dxui-performance-build-v1', $state['buildInputIdentity'], $state['toolchainIdentity'], $artifact) -join "`n")
            Write-ScopedReceipt -Path (Get-PerformanceBuildAttestationPath $Root x64 Release) -Identity $v1
        } $fixture
        Assert-Throws { Assert-PerformanceBuildAttestation -Root $fixture -Platform x64 -Configuration Release } 'old domain receipt was reused after the attestation protocol changed'
        $before['buildIdentityVersion'] = 1
        Assert-Throws { Get-PerformanceBuildAttestationIdentity -State $before -ArtifactIdentity ('A' * 64) } 'an old state was silently relabeled as a v2 build'
    }
}

Invoke-TestCase 'v2 measurement identity rejects missing old-version or unbound attestation metadata without rewriting historical reports' {
    $receipt = [ordered]@{ sourceCommit='a'; sourceFingerprint='b'; executableSha256='c'; benchmarkSha256='d'; harnessIdentity='e'; toolchainIdentity='f';
        dependencyIdentity='g'; environmentIdentity='env'; identityStatus='verifiable'; identityScope='full'; buildIdentityVersion=2;
        resolvedBuildIdentity=('1' * 64); buildInputIdentity=('2' * 64); buildArtifactIdentity=('3' * 64);
        fixture='fixture'; renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5;
        platform='x64'; configuration='Release'; nativeArchitecture='X64'; machine='m'; cpu='c'; os='o'; compiler=1; powerPolicy='p'; warpVersion='w'; warpSha256='sha' }
    $receipt['buildAttestationIdentity'] = Get-PerformanceBuildAttestationIdentity -State $receipt -ArtifactIdentity $receipt['buildArtifactIdentity']
    Assert-True (Test-PerformanceReceiptIdentity $receipt).verifiable 'the synthetic v2 structure is valid without claiming an actual native build'
    foreach ($name in @('buildIdentityVersion','resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity','identityScope')) {
        $missing = Copy-JsonValue $receipt
        [void]$missing.Remove($name)
        Assert-True (-not (Test-PerformanceReceiptIdentity $missing).verifiable) "missing $name was accepted"
    }
    $old = Copy-JsonValue $receipt
    $old.buildIdentityVersion = 1
    Assert-True (-not (Test-PerformanceReceiptIdentity $old).verifiable) 'a v1 report cannot qualify the new measurement harness'
    Assert-Equal 1 $old.buildIdentityVersion 'validation preserves historical receipt metadata'
    foreach ($name in @('resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','toolchainIdentity','buildAttestationIdentity')) {
        $unbound = Copy-JsonValue $receipt
        $unbound[$name] = ('4' * 64)
        Assert-True (-not (Test-PerformanceReceiptIdentity $unbound).verifiable) "unbound $name was accepted"
    }
}

Invoke-TestCase 'measurement guards reject source, executable, fixture, tools, dependencies, environment or power changes' {
    $before = [ordered]@{ sourceCommit='old source'; sourceDirty=$false; sourceFingerprint='original source'; executableSha256='original executable';
        benchmarkInputs=[ordered]@{ 'fixture.h'='original fixture' }; benchmarkSha256='original fixture hash'; buildInputIdentity='compiled inputs';
        buildIdentityVersion=2; resolvedBuildIdentity='expanded library and benchmark projects'; buildAttestationIdentity='attested build';
        buildArtifactIdentity='original artifacts'; identityStatus='verifiable'; identityScope='full'; harnessIdentity='original harness';
        toolchainIdentity='original tools'; dependencyIdentity='original dependencies'; environmentIdentity='original environment'; powerPolicy='original power'; warpSha256='original WARP' }
    Assert-PerformanceMeasurementState -Before $before -After (Copy-JsonValue $before)
    foreach ($key in $before.Keys) {
        $after = Copy-JsonValue $before
        $after[$key] = 'changed during native measurement'
        Assert-Throws { Assert-PerformanceMeasurementState -Before $before -After $after } "measurement mutation was accepted: $key"
    }
    $missing = Copy-JsonValue $before
    [void]$missing.Remove('executableSha256')
    Assert-Throws { Assert-PerformanceMeasurementState -Before $before -After $missing } 'a missing post-run identity was accepted'
}

Invoke-FixtureCase 'a project or imported option changed during measurement invalidates before-after build state' {
    param($root)
    Invoke-PerformanceBuildFixture $root {
        param($fixture)
        $module = Get-Module PerformancePolicy
        foreach ($path in @('src/DxUi.vcxproj','Tests/Embedded/DxUi.EmbeddedTests.vcxproj','Directory.Build.props','Build/Imported.targets','.build/toolchain/resolved-project.json')) {
            $before = & $module { param($Root) Get-PerformanceBuildState $Root x64 Release } $fixture
            $original = [IO.File]::ReadAllText((Join-Path $fixture $path))
            Set-FixtureFile $fixture $path ($original + ' changed during native timing')
            $after = & $module { param($Root) Get-PerformanceBuildState $Root x64 Release } $fixture
            Assert-Throws { Assert-PerformanceMeasurementState -Before $before -After $after } "mid-run project identity change was accepted: $path"
            Set-FixtureFile $fixture $path $original
        }
    }
}

Invoke-TestCase 'build-only toolchain identity cannot qualify a benchmark receipt' {
    $receipt = [ordered]@{ sourceCommit='a'; sourceFingerprint='b'; executableSha256='c'; benchmarkSha256='d'; harnessIdentity='e'; toolchainIdentity='f';
        dependencyIdentity='g'; environmentIdentity='env'; identityStatus='build-only'; identityScope='build-only';
        fixture='fixture'; renderer='WARP'; width=1280; height=720; dpi=96; controls=83; modelRows=1000; framesPerRound=40; roundCount=5;
        platform='x64'; configuration='Release'; nativeArchitecture='X64'; machine='m'; cpu='c'; os='o'; compiler=1; powerPolicy='p'; warpVersion='w'; warpSha256='sha' }
    $result = Test-PerformanceReceiptIdentity $receipt
    Assert-True (-not $result.verifiable -and $result.missing -contains 'identityStatus' -and $result.missing -contains 'identityScope') 'build-only identity was accepted as full measurement provenance'
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
    Assert-Equal 'dxui-paired-block-sign-flip-holm-v2' $policy.judgeVersion 'the policy names the judge version'
    Assert-Equal 'independent-ABBA-BAAB-v1' $policy.assignmentProtocol 'the policy names the independent assignment mechanism'
    Assert-True ($policy.approvedJudgeSha256 -match '^[A-Fa-f0-9]{64}$') 'the proposed judge source hash is reviewable'
    $source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../PerformanceComparison.psm1')) -replace "`r`n", "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $actualHash = [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($source))) } finally { $sha.Dispose() }
    Assert-Equal $actualHash $policy.approvedJudgeSha256 'the review target names the actual current judge bytes'
    $assignmentSource = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../PairedRun.psm1')) -replace "`r`n", "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $assignmentHash = [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($assignmentSource))) } finally { $sha.Dispose() }
    Assert-Equal $assignmentHash $policy.approvedAssignmentSha256 'the review target binds the actual assignment generator and replay source'
}
Complete-TestRun 'PerformancePolicy'
