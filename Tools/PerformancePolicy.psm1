# Trust checks for the migrated benchmark gate. A policy introduced by a candidate is never its own trust anchor.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'ScopedTesting.psm1')
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1')
Import-Module (Join-Path $PSScriptRoot 'Validation.psm1')

function Get-TrustedPerformancePolicy {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BaselineCommit, [Parameter(Mandatory)][string] $RepositoryRoot, [Parameter(Mandatory)][string] $CandidatePolicyPath)
    $path = 'Tools/PerformanceAcceptancePolicy.v1.json'
    $baseText = Get-GitBlobText -Root $RepositoryRoot -Commit $BaselineCommit -Path $path
    if ([string]::IsNullOrEmpty($baseText)) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured base does not contain the versioned acceptance policy; the candidate cannot introduce its own trust anchor.'; policySha256=$null }
    }
    try { $policy = $baseText | ConvertFrom-Json -AsHashtable }
    catch { return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The base policy is not valid JSON.'; policySha256=$null } }
    if ($policy.schemaVersion -ne 1 -or $policy.qualified -ne $true -or $policy.qualificationStatus -cne 'qualified') {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The base policy is not independently qualified; its verdict cannot establish a candidate pass.'; policySha256=$null }
    }
    if ([string]$policy.judgeVersion -notmatch '^dxui-[A-Za-z0-9-]+$' -or [string]$policy.approvedJudgeSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The qualified base policy does not bind an approved judge version and source hash.'; policySha256=$null }
    }
    $judgeText = Get-GitBlobText -Root $RepositoryRoot -Commit $BaselineCommit -Path 'Tools/PerformanceComparison.psm1'
    if ([string]::IsNullOrEmpty($judgeText)) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The qualified base policy judge source cannot be recovered.'; policySha256=$null }
    }
    $judgeSource = $judgeText -replace "`r`n", "`n"
    $judgeHasher = [Security.Cryptography.SHA256]::Create()
    try { $baseJudgeSha = [Convert]::ToHexString($judgeHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($judgeSource))) } finally { $judgeHasher.Dispose() }
    if ($baseJudgeSha -cne [string]$policy.approvedJudgeSha256) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base judge source does not match the hash approved by its policy.'; policySha256=$null; baseJudgeSha256=$baseJudgeSha }
    }
    try {
        $judgeModule = New-Module -Name "DxUiTrustedJudge_$([guid]::NewGuid().ToString('N'))" -ScriptBlock ([scriptblock]::Create($judgeSource))
        if (-not $judgeModule.ExportedFunctions.ContainsKey('Get-PairedPerformanceJudgeVersion')) { throw 'The base judge has no exported version identity.' }
        $baseJudgeVersion = & $judgeModule { Get-PairedPerformanceJudgeVersion }
    } catch {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base judge version cannot be verified.'; policySha256=$null }
    }
    if ([string]$baseJudgeVersion -cne [string]$policy.judgeVersion) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base judge version does not match the version approved by its policy.'; policySha256=$null; baseJudgeVersion=$baseJudgeVersion }
    }
    if ([string]$policy['approvedAssignmentSha256'] -notmatch '^[A-Fa-f0-9]{64}$' -or
        [string]::IsNullOrWhiteSpace([string]$policy['assignmentProtocol'])) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The qualified base policy does not bind its assignment protocol and generator source.'; policySha256=$null }
    }
    try {
        if (-not $judgeModule.ExportedFunctions.ContainsKey('Get-PairedAssignmentProtocol')) { throw 'The base judge has no assignment protocol identity.' }
        $baseProtocol = & $judgeModule { Get-PairedAssignmentProtocol }
    } catch {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base assignment protocol cannot be verified.'; policySha256=$null }
    }
    if ([string]$baseProtocol -cne [string]$policy['assignmentProtocol']) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base assignment protocol differs from the protocol approved by its policy.'; policySha256=$null }
    }
    $assignmentText = Get-GitBlobText -Root $RepositoryRoot -Commit $BaselineCommit -Path 'Tools/PairedRun.psm1'
    if ([string]::IsNullOrEmpty($assignmentText)) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base assignment generator source cannot be recovered.'; policySha256=$null }
    }
    $assignmentSource = $assignmentText -replace "`r`n", "`n"
    $assignmentHasher = [Security.Cryptography.SHA256]::Create()
    try { $baseAssignmentSha = [Convert]::ToHexString($assignmentHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($assignmentSource))) }
    finally { $assignmentHasher.Dispose() }
    if ($baseAssignmentSha -cne [string]$policy['approvedAssignmentSha256']) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base assignment generator does not match the hash approved by its policy.'; policySha256=$null }
    }
    $candidateAssignmentPath = Join-Path (Split-Path -Parent $CandidatePolicyPath) 'PairedRun.psm1'
    if (-not (Test-Path -LiteralPath $candidateAssignmentPath -PathType Leaf)) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The candidate assignment generator source is missing.'; policySha256=$null }
    }
    $candidateAssignmentSource = [IO.File]::ReadAllText($candidateAssignmentPath) -replace "`r`n", "`n"
    $assignmentHasher = [Security.Cryptography.SHA256]::Create()
    try { $candidateAssignmentSha = [Convert]::ToHexString($assignmentHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($candidateAssignmentSource))) }
    finally { $assignmentHasher.Dispose() }
    if ($candidateAssignmentSha -cne $baseAssignmentSha) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The candidate changes the approved assignment generator; independent randomization qualification must be reviewed.'; policySha256=$null }
    }
    $canonicalBase = ConvertTo-Json -InputObject $policy -Depth 32 -Compress
    $bytes = [Text.Encoding]::UTF8.GetBytes($canonicalBase)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = [Convert]::ToHexString($sha.ComputeHash($bytes)) } finally { $sha.Dispose() }
    if (-not (Test-Path -LiteralPath $CandidatePolicyPath -PathType Leaf)) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='Candidate policy file is missing.'; policySha256=$hash }
    }
    try { $candidatePolicy = Get-Content -Raw -LiteralPath $CandidatePolicyPath | ConvertFrom-Json -AsHashtable }
    catch { return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The candidate policy is not valid JSON.'; policySha256=$hash } }
    $candidateCanonical = ConvertTo-Json -InputObject $candidatePolicy -Depth 32 -Compress
    $candidateSha = [Security.Cryptography.SHA256]::Create()
    try { $candidateHash = [Convert]::ToHexString($candidateSha.ComputeHash([Text.Encoding]::UTF8.GetBytes($candidateCanonical))) } finally { $candidateSha.Dispose() }
    if ($candidateHash -cne $hash) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The candidate changes the trusted policy; both judges must be compared on byte-identical reports and the policy reviewed.'; policySha256=$hash; candidatePolicySha256=$candidateHash }
    }
    return [ordered]@{ trusted=$true; status='trusted'; reason='The measured base contains an independently qualified versioned policy.'; policySha256=$hash; policy=$policy }
}

function Normalize-PerformanceBuildPath([string] $Value, [string] $Root) {
    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    foreach ($rootVariant in @($rootPath, $rootPath.Replace('\','/')) | Select-Object -Unique) {
        if ($rootVariant) {
            $pattern = [regex]::Escape($rootVariant) + '(?=$|[\\/;])'
            $Value = [regex]::Replace($Value, $pattern, '<DxUiRoot>', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
        }
    }
    return $Value
}

function Get-PreprocessedProjectIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Text, [Parameter(Mandatory)][string] $Root)
    if ([string]::IsNullOrWhiteSpace($Text)) { throw 'MSBuild preprocessed project output is empty.' }
    $canonical = $Text -replace "`r`n", "`n" -replace "`r", "`n"
    try { [xml]$project = $canonical }
    catch { throw 'MSBuild preprocessed project output is malformed XML.' }
    if ($null -eq $project.DocumentElement -or $project.DocumentElement.LocalName -cne 'Project') {
        throw 'MSBuild preprocessed output root is not a Project element.'
    }
    $canonical = Normalize-PerformanceBuildPath $canonical $Root
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))) }
    finally { $sha.Dispose() }
}

function Get-PreprocessedProjectFileIdentity {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MSBuildPath,
        [Parameter(Mandatory)][string] $ProjectPath,
        [Parameter(Mandatory)][string] $Configuration,
        [Parameter(Mandatory)][string] $Platform,
        [Parameter(Mandatory)][string] $OutputRoot,
        [Parameter(Mandatory)][string] $Root,
        [Collections.IDictionary] $SolutionProperties,
        [switch] $PassThru
    )
    if (-not (Test-Path -LiteralPath $MSBuildPath -PathType Leaf)) { throw "Resolved MSBuild executable is missing: $MSBuildPath" }
    if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Project to preprocess is missing: $ProjectPath" }
    $temporaryPath = Join-Path ([IO.Path]::GetTempPath()) ("DxUi-expanded-project-$([guid]::NewGuid().ToString('N')).xml")
    try {
        $arguments = @($ProjectPath, '/nologo', "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$OutputRoot", "/pp:$temporaryPath") + @(ConvertTo-PerformanceMSBuildProperties $SolutionProperties)
        $output = @(& $MSBuildPath @arguments 2>&1)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) { throw "MSBuild project preprocessing failed with exit code $exitCode. $($output -join ' ')" }
        if (-not (Test-Path -LiteralPath $temporaryPath -PathType Leaf)) { throw 'MSBuild succeeded without producing preprocessed project output.' }
        $text = [IO.File]::ReadAllText($temporaryPath)
        $identity = Get-PreprocessedProjectIdentity -Text $text -Root $Root
        if ($PassThru) { return [ordered]@{ text=$text; identity=$identity } }
        return $identity
    }
    finally {
        if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) { Remove-Item -LiteralPath $temporaryPath -Force }
    }
}

function ConvertTo-PerformanceMSBuildProperties {
    param([Collections.IDictionary] $Properties)
    if ($null -eq $Properties) { return }
    foreach ($name in @($Properties.Keys | Sort-Object -CaseSensitive)) {
        # MSBuild's property-list parser treats these characters specially. Escape literal XML quotes/newlines too;
        # native argument quoting must not change CurrentSolutionConfigurationContents on the way to evaluation.
        $value = [regex]::Replace([string]$Properties[$name], '[%*?@$();''",\r\n]', {
            param($Match) '%' + ([int][char]$Match.Value).ToString('X2')
        })
        "/p:${name}=$value"
    }
}

function Get-PerformanceSolutionConfiguration {
    param([string] $MSBuildPath, [string] $SolutionPath, [string] $Configuration, [string] $Platform)
    # Use the selected engine's own solution parser/generator. The private generator is deliberately checked rather
    # than approximating its XML or executing a solution target; an unsupported engine fails attestation closed.
    $assemblyPath = Join-Path (Split-Path -Parent $MSBuildPath) 'Microsoft.Build.dll'
    if (-not (Test-Path -LiteralPath $assemblyPath -PathType Leaf)) { throw 'The selected MSBuild solution parser is missing.' }
    $assemblySha = (Get-FileHash -LiteralPath $assemblyPath -Algorithm SHA256).Hash
    # The loaded assembly outlives Import-Module -Force. Keep its byte identity in the same process lifetime so a
    # wrapper/module reload cannot silently associate previously loaded parser code with replacement file bytes.
    $cacheKey = 'DxUi.Performance.MSBuildAssembly.v2'
    $cached = [AppDomain]::CurrentDomain.GetData($cacheKey)
    if ($null -ne $cached) {
        if ($cached.path -ine $assemblyPath -or $cached.sha -cne $assemblySha) {
            throw 'The loaded MSBuild parser changed; restart the build driver before attesting its replacement.'
        }
    } else {
        $assembly = [Reflection.Assembly]::LoadFrom($assemblyPath)
        if ($assembly.Location -ine $assemblyPath -or (Get-FileHash -LiteralPath $assemblyPath -Algorithm SHA256).Hash -cne $assemblySha) {
            throw 'The selected MSBuild parser differs from the loaded assembly.'
        }
        $cached = @{ path=$assemblyPath; sha=$assemblySha; assembly=$assembly }
        [AppDomain]::CurrentDomain.SetData($cacheKey, $cached)
    }
    $assembly = $cached.assembly
    $solution = $assembly.GetType('Microsoft.Build.Construction.SolutionFile').GetMethod('Parse').Invoke($null, @($SolutionPath))
    $selected = @($solution.SolutionConfigurations | Where-Object { $_.FullName -ceq "$Configuration|$Platform" })
    if ($selected.Count -ne 1) { throw 'The selected solution configuration is missing or repeated.' }
    $generator = $assembly.GetType('Microsoft.Build.Construction.SolutionProjectGenerator')
    $method = $generator.GetMethod('GetSolutionConfiguration', [Reflection.BindingFlags]'Static,NonPublic')
    if ($null -eq $method) { throw 'The selected MSBuild engine cannot expose its exact solution configuration.' }
    $escaped = $method.Invoke($null, [object[]]@($solution.PSObject.BaseObject, $selected[0].PSObject.BaseObject))
    return $assembly.GetType('Microsoft.Build.Evaluation.ProjectCollection').GetMethod('Unescape').Invoke($null, @($escaped))
}

function Get-PerformanceSolutionGlobals {
    param([string] $MSBuildPath, [string] $Root, [string] $Configuration, [string] $Platform)
    $solutionPath = [IO.Path]::GetFullPath((Join-Path $Root 'DxUi.sln'))
    if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'The benchmark build solution is missing.' }
    $errors = @(Test-DxUiSolutionConfigurations $solutionPath)
    if ($errors.Count) { throw "Unsupported solution build mappings: $($errors -join '; ')" }
    $content = Get-PerformanceSolutionConfiguration $MSBuildPath $solutionPath $Configuration $Platform
    [xml]$xml = $content
    if ($xml.DocumentElement.LocalName -cne 'SolutionConfiguration') { throw 'MSBuild returned an invalid solution configuration.' }
    foreach ($relative in @('src/DxUi.vcxproj','Tests/Embedded/DxUi.EmbeddedTests.vcxproj')) {
        $path = [IO.Path]::GetFullPath((Join-Path $Root $relative))
        $nodes = @($xml.DocumentElement.SelectNodes('ProjectConfiguration') | Where-Object { $_.GetAttribute('AbsolutePath') -ieq $path })
        if ($nodes.Count -ne 1 -or $nodes[0].GetAttribute('BuildProjectInSolution') -cne 'True' -or
                $nodes[0].FirstChild.Value -cne "$Configuration|$Platform") {
            throw "The solution does not build $relative in the exact requested configuration."
        }
    }
    $properties = [ordered]@{ BuildingSolutionFile='true'; CurrentSolutionConfigurationContents=$content;
        SolutionDir=([IO.Path]::GetDirectoryName($solutionPath) + [IO.Path]::DirectorySeparatorChar);
        SolutionExt=[IO.Path]::GetExtension($solutionPath); SolutionFileName=[IO.Path]::GetFileName($solutionPath);
        SolutionName=[IO.Path]::GetFileNameWithoutExtension($solutionPath); SolutionPath=$solutionPath }
    return [ordered]@{ properties=$properties;
        identity=(Get-ScopedDigest (@('dxui-solution-v2', (Get-FileHash -LiteralPath $solutionPath -Algorithm SHA256).Hash,
            (Normalize-PerformanceBuildPath $content $Root)) -join "`n")) }
}

function Get-PerformanceItemValues {
    param([object] $Item, [string] $Root, [switch] $Profile)
    $values = [ordered]@{}
    $properties = if ($Item -is [Collections.IDictionary]) { $Item.Keys } else { $Item.PSObject.Properties.Name }
    # Clock metadata is not an input; the synthetic evaluation item's defining file has a temporary name.
    $ignored = @('ModifiedTime','CreatedTime','AccessedTime','DefiningProjectFullPath','DefiningProjectDirectory','DefiningProjectName','DefiningProjectExtension')
    if ($Profile) { $ignored += @('Identity','FullPath','RootDir','Filename','Extension','RelativeDir','Directory','RecursiveDir','OriginalItemSpec') }
    foreach ($name in @($properties | Sort-Object -CaseSensitive)) {
        if ($name -in $ignored) { continue }
        $value = if ($Item -is [Collections.IDictionary]) { $Item[$name] } else { $Item.PSObject.Properties[$name].Value }
        $values[$name] = Normalize-PerformanceBuildPath ([string]$value) $Root
    }
    return $values
}

function Assert-PerformanceItemMetadata {
    param([Collections.IDictionary] $Values, [string[]] $Required, [string] $Kind)
    foreach ($name in $Required) {
        if (-not $Values.Contains($name) -or [string]::IsNullOrWhiteSpace([string]$Values[$name])) {
            throw "Resolved $Kind item has no effective $name metadata."
        }
    }
    if ($Values.Contains('ExcludedFromBuild') -and $Values['ExcludedFromBuild'] -eq 'true') {
        throw "Excluded $Kind items are unsupported in performance build attestation."
    }
}

function Get-ResolvedCompileItemsIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object[]] $Items, [Parameter(Mandatory)][string] $Root)
    $rows = [Collections.Generic.List[object]]::new()
    $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($item in $Items) {
        $values = Get-PerformanceItemValues -Item $item -Root $Root
        Assert-PerformanceItemMetadata -Values $values -Required @('Identity','AdditionalIncludeDirectories','AdditionalOptions','PreprocessorDefinitions',
            'LanguageStandard','RuntimeLibrary','Optimization','WarningLevel','TreatWarningAsError','SDLCheck','ConformanceMode','ExceptionHandling') -Kind ClCompile
        if (-not $paths.Add($values['Identity'])) { throw 'Duplicate resolved compiler paths are unsupported.' }
        $rows.Add($values)
    }
    if (-not $rows.Count) { throw 'MSBuild resolved no compiler items.' }
    $canonical = @($rows.ToArray() | Sort-Object { $_['Identity'] }) | ConvertTo-Json -Depth 12 -Compress -AsArray
    return Get-ScopedDigest ("dxui-resolved-cl-v2`n" + $canonical)
}

function Get-PerformanceToolPropertyNames {
    param([xml] $Project)
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($definition in $Project.SelectNodes("/*[local-name()='Project']/*[local-name()='Target' or local-name()='UsingTask']")) {
        # Conditions and target-time metadata assignments also consume globals. Binding only task arguments would miss
        # a property such as UseDynamicDebugging that changes a compiler option inside an otherwise identical target.
        foreach ($match in [regex]::Matches($definition.OuterXml, '\$\(([A-Za-z_][A-Za-z0-9_.]*)\)')) { [void]$names.Add($match.Groups[1].Value) }
    }
    foreach ($name in @('PlatformToolset','VCToolsInstallDir','VCToolsVersion','WindowsSdkDir','WindowsTargetPlatformVersion','PreferredToolArchitecture',
            'DxUiOutputRoot','VCTargetsPath','ConfigurationType','VCToolArchitecture','WindowsSDKToolArchitecture','FxCompileToolArchitecture','FxCompileToolExe',
            'FXCToolPath','CLToolPath','CLToolExe','LinkToolPath','LinkToolExe','LibToolPath','LibToolExe','ForceImportAfterCppTargets','MultiProcFXC',
            'IncludePath','LibraryPath','ExecutablePath','ReferencePath','ExcludePath','EnableASAN','UseDebugLibraries','LinkIncremental','LibAdditionalOptions',
            'IntDir','OutDir','VSInstallDir','UniversalCRTSdkDir','UCRTVersion','NETFXKitsDir','ExternalIncludePath',
            'BuildingSolutionFile','CurrentSolutionConfigurationContents','SolutionDir','SolutionExt','SolutionFileName','SolutionName','SolutionPath','Configuration','Platform')) {
        [void]$names.Add($name)
    }
    return @($names | Sort-Object -CaseSensitive)
}

function Get-PerformanceProjectProfile {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][ValidateSet('library','benchmark')][string] $Role,
        [Parameter(Mandatory)][string] $ExpandedText, [Parameter(Mandatory)][Collections.IDictionary] $Properties,
        [Parameter(Mandatory)][Collections.IDictionary] $Items)
    $expandedIdentity = Get-PreprocessedProjectIdentity -Text $ExpandedText -Root $Root
    [xml]$project = $ExpandedText
    $propertyValues = [ordered]@{}
    foreach ($name in Get-PerformanceToolPropertyNames $project) {
        if (-not $Properties.Contains($name)) { throw "Resolved $Role build omits the $name property." }
        $propertyValues[$name] = Normalize-PerformanceBuildPath ([string]$Properties[$name]) $Root
    }
    foreach ($name in @('PlatformToolset','VCToolsInstallDir','VCToolsVersion','WindowsSdkDir','WindowsTargetPlatformVersion','PreferredToolArchitecture',
            'DxUiOutputRoot','VCTargetsPath','ConfigurationType','VCToolArchitecture','WindowsSDKToolArchitecture','IncludePath','LibraryPath','ExecutablePath')) {
        if ([string]::IsNullOrWhiteSpace([string]$propertyValues[$name])) { throw "Resolved $Role build has no $name property." }
    }
    if ($propertyValues['ForceImportAfterCppTargets']) { throw 'A real ForceImportAfterCppTargets override cannot be replaced by a performance metadata probe.' }
    $expectedType = if ($Role -eq 'library') { 'StaticLibrary' } else { 'Application' }
    if ($propertyValues['ConfigurationType'] -cne $expectedType) { throw "Unsupported $Role configuration type." }
    foreach ($kind in @('ClCompile','FxCompile','Link','Lib','ProjectReference','CustomBuild','ResourceCompile','Midl','MASM','ARMASM','BuildMacro','PreBuildEvent','PreLinkEvent','PostBuildEvent')) {
        if (-not $Items.Contains($kind)) { throw "Resolved $Role build omits the $kind item inventory." }
    }
    foreach ($kind in @('CustomBuild','ResourceCompile','Midl','MASM','ARMASM','BuildMacro')) {
        if (@($Items[$kind]).Count) { throw "Unsupported $kind items in the $Role performance build." }
    }
    foreach ($kind in @('PreBuildEvent','PreLinkEvent','PostBuildEvent')) {
        $events = @($Items[$kind])
        $probeCount = 0
        foreach ($item in $events) {
            $event = Get-PerformanceItemValues $item $Root
            if ($event['Identity'] -ceq "DxUiPerformance${kind}Probe") { $probeCount++ }
            if ($event.Contains('Command') -and -not [string]::IsNullOrWhiteSpace([string]$event['Command'])) { throw "Unsupported $kind command in performance build attestation." }
        }
        if ($probeCount -ne 1) { throw "Missing $kind metadata probe." }
    }
    $clItems = @($Items['ClCompile'])
    $compileItemsIdentity = Get-ResolvedCompileItemsIdentity -Items $clItems -Root $Root
    $clProfiles = @($clItems | ForEach-Object { (Get-PerformanceItemValues $_ $Root -Profile) | ConvertTo-Json -Depth 12 -Compress } | Select-Object -Unique)
    # A set of profiles would hide a /O2-/Od swap between two files. Support only one uniform profile per project.
    if ($clProfiles.Count -ne 1) { throw "Heterogeneous ClCompile options are unsupported in the $Role performance build." }
    $fxProfiles = [ordered]@{}
    $fxRows = @($Items['FxCompile'] | ForEach-Object { Get-PerformanceItemValues $_ $Root })
    $expectedFx = [ordered]@{ 'Rendering\CompositeVS.hlsl'='Vertex'; 'Rendering\CompositePS.hlsl'='Pixel' }
    if ($Role -eq 'library') {
        if ($fxRows.Count -ne $expectedFx.Count) { throw 'The library shader inventory must contain the two explicit composite shader roles.' }
        foreach ($path in $expectedFx.Keys) {
            $shader = @($fxRows | Where-Object { $_['Identity'].Replace('/','\') -ieq $path })
            if ($shader.Count -ne 1) { throw "Missing or repeated shader role: $path" }
            Assert-PerformanceItemMetadata $shader[0] @('ShaderType','ShaderModel','EntryPointName','AdditionalOptions','HeaderFileOutput','ObjectFileOutput','VariableName') FxCompile
            if ($shader[0]['ShaderType'] -cne $expectedFx[$path]) { throw "The $path shader role has an unsupported ShaderType." }
            $fxProfiles[$expectedFx[$path]] = Get-PerformanceItemValues $shader[0] $Root -Profile
        }
    } elseif ($fxRows.Count) { throw 'Benchmark-local shader compilation is unsupported.' }
    $linkProfiles = [ordered]@{}
    foreach ($kind in @('Link','Lib')) {
        $probes = @($Items[$kind])
        if ($probes.Count -ne 1) { throw "Missing or extra $kind metadata probe items." }
        $values = Get-PerformanceItemValues $probes[0] $Root
        if ($values['Identity'] -cne "DxUiPerformance${kind}Probe") { throw "Unexpected $kind metadata probe identity." }
        Assert-PerformanceItemMetadata $values @('OutputFile','TargetMachine') $kind
        $linkProfiles[$kind] = Get-PerformanceItemValues $values $Root -Profile
    }
    $references = @($Items['ProjectReference'])
    if (($Role -eq 'library' -and $references.Count) -or ($Role -eq 'benchmark' -and $references.Count -ne 1)) { throw 'Unsupported performance project reference inventory.' }
    $referenceProfiles = @($references | ForEach-Object {
        $values = Get-PerformanceItemValues $_ $Root
        if ([IO.Path]::GetFullPath((Join-Path (Join-Path $Root 'Tests/Embedded') $values['Identity'])) -ine [IO.Path]::GetFullPath((Join-Path $Root 'src/DxUi.vcxproj'))) {
            throw 'The benchmark must reference its own library project.'
        }
        Get-PerformanceItemValues $values $Root -Profile
    })
    # Flags assigned inside a target are not visible to evaluation-only item queries. Bind every target and task definition
    # to the comparison profile; top-level source registration remains solely in the per-side expanded project identity.
    $targets = @($project.SelectNodes("/*[local-name()='Project']/*[local-name()='Target' or local-name()='UsingTask']") | ForEach-Object { $_.OuterXml }) -join "`n"
    $profile = [ordered]@{ properties=$propertyValues; cl=$clProfiles[0]; fx=$fxProfiles; link=$linkProfiles; references=$referenceProfiles;
        targets=(Normalize-PerformanceBuildPath $targets $Root) }
    $resolvedRows = [ordered]@{}
    foreach ($kind in @($Items.Keys | Sort-Object -CaseSensitive)) {
        $resolvedRows[$kind] = @($Items[$kind] | ForEach-Object { Get-PerformanceItemValues $_ $Root } | Sort-Object { $_['Identity'] })
    }
    return [ordered]@{ expandedProjectIdentity=$expandedIdentity; compileItemsIdentity=$compileItemsIdentity;
        resolvedItemsIdentity=(Get-ScopedDigest ("dxui-resolved-items-v2`n" + ($resolvedRows | ConvertTo-Json -Depth 12 -Compress)));
        profileIdentity=(Get-ScopedDigest ("dxui-project-profile-v2`n" + ($profile | ConvertTo-Json -Depth 12 -Compress))) }
}

function Get-PerformanceProjectEvaluation {
    param([string] $MSBuildPath, [string] $ProjectPath, [string] $Configuration, [string] $Platform, [string] $OutputRoot, [string] $Root, [string] $Role)
    # Default-output build.ps1 uses this same target context. A paired baseline must not inherit the harness checkout's
    # MSBuildStartupDirectory, which imported build logic is allowed to consume.
    Push-Location -LiteralPath $Root
    try { return Get-PerformanceProjectEvaluationCore @PSBoundParameters }
    finally { Pop-Location }
}

function Assert-PerformanceProbeItems {
    param([Collections.IDictionary] $Original, [Collections.IDictionary] $Probed, [string[]] $Kinds, [string] $Root)
    foreach ($kind in $Kinds) {
        if (-not $Original.Contains($kind) -or -not $Probed.Contains($kind)) { throw "The metadata probe omits the $kind inventory." }
        $before = @($Original[$kind] | ForEach-Object { Get-PerformanceItemValues $_ $Root } | Sort-Object { $_['Identity'] }) | ConvertTo-Json -Depth 12 -Compress -AsArray
        $after = @($Probed[$kind] | ForEach-Object { Get-PerformanceItemValues $_ $Root } | Sort-Object { $_['Identity'] }) | ConvertTo-Json -Depth 12 -Compress -AsArray
        if ($before -cne $after) { throw "The metadata probe changes the effective $kind inventory or options; this customization cannot be attested." }
    }
}

function Get-PerformanceProjectEvaluationCore {
    param([string] $MSBuildPath, [string] $ProjectPath, [string] $Configuration, [string] $Platform, [string] $OutputRoot, [string] $Root, [string] $Role)
    $solution = Get-PerformanceSolutionGlobals $MSBuildPath $Root $Configuration $Platform
    $expanded = Get-PreprocessedProjectFileIdentity -MSBuildPath $MSBuildPath -ProjectPath $ProjectPath -Configuration $Configuration -Platform $Platform -OutputRoot $OutputRoot -Root $Root -SolutionProperties $solution.properties -PassThru
    [xml]$project = $expanded['text']
    $names = Get-PerformanceToolPropertyNames $project
    $arguments = @($ProjectPath, '/nologo', "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$OutputRoot") + @(ConvertTo-PerformanceMSBuildProperties $solution.properties)
    $properties = [ordered]@{}
    # The installed target closure references more than a thousand properties. Bound command-line size on Windows.
    for ($offset = 0; $offset -lt $names.Count; $offset += 200) {
        $batch = $names[$offset..([Math]::Min($offset + 199, $names.Count - 1))]
        $json = @(& $MSBuildPath @arguments "-getProperty:$($batch -join ',')" 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "MSBuild could not evaluate $Role toolchain properties. $($json -join ' ')" }
        $values = ($json -join "`n" | ConvertFrom-Json -AsHashtable)['Properties']
        foreach ($name in $batch) {
            if (-not $values.Contains($name)) { throw "MSBuild omitted the requested $name property." }
            $properties[$name] = $values[$name]
        }
    }
    if ($properties['ForceImportAfterCppTargets']) { throw 'A real ForceImportAfterCppTargets override cannot be replaced by a performance metadata probe.' }
    $originalKinds = @('ClCompile','FxCompile','ProjectReference','CustomBuild','ResourceCompile','Midl','MASM','ARMASM','BuildMacro')
    $json = @(& $MSBuildPath @arguments "-getItem:$($originalKinds -join ',')" 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "MSBuild could not evaluate the unmodified $Role compilation inventory. $($json -join ' ')" }
    $originalItems = ($json -join "`n" | ConvertFrom-Json -AsHashtable)['Items']
    $probePath = Join-Path ([IO.Path]::GetTempPath()) ("DxUi-performance-probe-$([guid]::NewGuid().ToString('N')).targets")
    try {
        $probeKinds = @('Link','Lib','PreBuildEvent','PreLinkEvent','PostBuildEvent')
        $probeItems = @($probeKinds | ForEach-Object { "<$_ Include=`"DxUiPerformance${_}Probe`" />" }) -join ''
        [IO.File]::WriteAllText($probePath, "<Project xmlns=`"http://schemas.microsoft.com/developer/msbuild/2003`"><ItemGroup>$probeItems</ItemGroup></Project>", [Text.UTF8Encoding]::new($false))
        $itemNames = @('ClCompile','FxCompile','Link','Lib','ProjectReference','CustomBuild','ResourceCompile','Midl','MASM','ARMASM','BuildMacro') + $probeKinds[2..4]
        $json = @(& $MSBuildPath @arguments "/p:ForceImportAfterCppTargets=$probePath" "-getItem:$($itemNames -join ',')" 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "MSBuild could not evaluate $Role compiler/linker metadata. $($json -join ' ')" }
        $items = ($json -join "`n" | ConvertFrom-Json -AsHashtable)['Items']
        Assert-PerformanceProbeItems -Original $originalItems -Probed $items -Kinds $originalKinds -Root $Root
        $profile = Get-PerformanceProjectProfile -Root $Root -Role $Role -ExpandedText $expanded['text'] -Properties $properties -Items $items
        $inputs = Get-PerformanceNativeInputClosure -Root $Root -Role $Role -Platform $Platform -Configuration $Configuration -Properties $properties -Items $items
        return [ordered]@{ properties=$properties; profile=$profile; inputs=$inputs; solutionIdentity=$solution.identity; projectSha256=(Get-FileHash -LiteralPath $ProjectPath -Algorithm SHA256).Hash }
    } finally {
        if (Test-Path -LiteralPath $probePath -PathType Leaf) { Remove-Item -LiteralPath $probePath -Force }
    }
}

function Resolve-PerformanceInputPath([string] $Value, [string] $Base) {
    $value = $Value.Trim().Trim('"')
    if (-not $value -or $value -match '[*?@$%]' -or $value.Contains('"')) { throw "Unsupported native input path: $Value" }
    return [IO.Path]::GetFullPath($value, [IO.Path]::GetFullPath($Base)).TrimEnd('\','/')
}

function Test-PerformancePathWithin([string] $Path, [string] $Directory) {
    $prefix = [IO.Path]::GetFullPath($Directory).TrimEnd('\','/')
    return $Path -ieq $prefix -or $Path.StartsWith($prefix + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-PerformanceInputOptions([string] $Options) {
    # Response files and path-bearing switches can select unenumerated files. Effective metadata is checked separately;
    # do not interpret arbitrary command lines or silently accept input/output overrides in AdditionalOptions/CL/_CL_.
    if ($Options -match '@' -or $Options -match '(?i)(^|\s)[/-](I(?!GNORE\b|NCREMENTAL\b|NFERASANLIBS\b)|FI|FU|AI|Fo|Fd|Fp(?!:)|Fe|Fh|Fa|FR|Tc|Tp|Yc|Yu|B1|B2|sourceDependencies|scanDependencies|external:I|LIBPATH|DEF|DEFAULTLIB|WHOLEARCHIVE|reference|headerUnit|ifcSearchDir)') {
        throw 'Native response files and path-bearing option overrides are unsupported by performance attestation.'
    }
    foreach ($token in [regex]::Matches($Options, '(?:[^\s"]+|"[^"]*")+')) {
        if ($token.Value.Trim('"') -notmatch '^[/-]') { throw 'Bare native option operands are unsupported; use the attested effective metadata.' }
    }
}

function Get-PerformanceNativeInputClosure {
    param([string] $Root, [string] $Role, [string] $Platform, [string] $Configuration,
        [Collections.IDictionary] $Properties, [Collections.IDictionary] $Items)
    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    $projectDirectory = Join-Path $rootPath $(if ($Role -eq 'library') { 'src' } else { 'Tests/Embedded' })
    $projectName = if ($Role -eq 'library') { 'DxUi' } else { 'DxUi.EmbeddedTests' }
    $intDir = Join-Path $rootPath ".build/Intermediate/$Platform/$Configuration/$projectName"
    $outDir = Join-Path $rootPath ".build/$Platform/$Configuration"
    foreach ($entry in @(@('IntDir',$intDir), @('OutDir',$outDir), @('DxUiOutputRoot',(Join-Path $rootPath '.build')))) {
        if ([string]::IsNullOrWhiteSpace([string]$Properties[$entry[0]]) -or (Resolve-PerformanceInputPath $Properties[$entry[0]] $projectDirectory) -ine [IO.Path]::GetFullPath($entry[1]).TrimEnd('\','/')) {
            throw "The supported native build requires its default $($entry[0]) path."
        }
    }
    # These are the selected installation's supported default include/library search roots, not arbitrary external trees.
    $headers = [Collections.Generic.List[string]]::new()
    $libraries = [Collections.Generic.List[string]]::new()
    foreach ($relative in @('include','atlmfc/include')) { $headers.Add((Join-Path $Properties['VCToolsInstallDir'] $relative)) }
    foreach ($relative in @("lib/$Platform","atlmfc/lib/$Platform")) { $libraries.Add((Join-Path $Properties['VCToolsInstallDir'] $relative)) }
    $sdk = [string]$Properties['WindowsTargetPlatformVersion']
    foreach ($relative in @('um','shared','ucrt','winrt','cppwinrt')) { $headers.Add((Join-Path $Properties['WindowsSdkDir'] "Include/$sdk/$relative")) }
    foreach ($relative in @('um','ucrt')) { $libraries.Add((Join-Path $Properties['WindowsSdkDir'] "Lib/$sdk/$relative/$Platform")) }
    if ($Properties['UniversalCRTSdkDir'] -and $Properties['UCRTVersion']) {
        $headers.Add((Join-Path $Properties['UniversalCRTSdkDir'] "Include/$($Properties['UCRTVersion'])/ucrt"))
        $libraries.Add((Join-Path $Properties['UniversalCRTSdkDir'] "Lib/$($Properties['UCRTVersion'])/ucrt/$Platform"))
    }
    if ($Properties['VSInstallDir']) {
        foreach ($relative in @('VC/Auxiliary/VS/include','VC/Auxiliary/VS/UnitTest/include')) { $headers.Add((Join-Path $Properties['VSInstallDir'] $relative)) }
        foreach ($relative in @("VC/Auxiliary/VS/lib/$Platform",'VC/Auxiliary/VS/UnitTest/lib')) { $libraries.Add((Join-Path $Properties['VSInstallDir'] $relative)) }
    }
    if ($Properties['NETFXKitsDir']) {
        $headers.Add((Join-Path $Properties['NETFXKitsDir'] 'Include/um'))
        $libraries.Add((Join-Path $Properties['NETFXKitsDir'] "lib/um/$Platform"))
    }
    $headers = @($headers | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\','/') } | Sort-Object -Unique)
    $libraries = @($libraries | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\','/') } | Sort-Object -Unique)
    $triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
    $dependencyRoot = Join-Path $rootPath ".build/vcpkg_installed/$Platform/$triplet"
    $nativeDirectories = @('src','include','Tests/Embedded','Tests/Support','Samples/EmbeddedControls','Samples/ComplexUi')
    # Reuse scoped Git discovery, but retain every byte in the supported native input trees, including extensionless files
    # and prose that could be included explicitly. Ignored inputs and reparse points cannot become invisible dependencies.
    $tracked = & (Get-Module ScopedTesting) { param($Root) Get-ScopedTrackedPaths $Root } $rootPath
    $owned = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $tracked) { [void]$owned.Add($relative.Replace('\','/')) }
    $rows = [Collections.Generic.List[string]]::new()
    $inputFiles = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $nativeDirectories) {
        $directory = Join-Path $rootPath $relative
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) { continue }
        if ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse native input roots are unsupported: $directory" }
        foreach ($entry in Get-ChildItem -LiteralPath $directory -Recurse -Force -ErrorAction Stop) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse native inputs are unsupported: $($entry.FullName)" }
            if ($entry.PSIsContainer) { continue }
            $path = $entry.FullName.Substring($rootPath.Length).TrimStart('\','/').Replace('\','/')
            if (-not $owned.Contains($path)) { throw "Ignored or unowned native input is unsupported: $path" }
            $sha = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
            $inputFiles[$entry.FullName] = $sha
            $rows.Add("$path $sha")
            if ($entry.Extension -in @('.cpp','.h','.hpp','.inl','.hlsl') -or -not $entry.Extension) {
                $text = [IO.File]::ReadAllText($entry.FullName)
                $text = [regex]::Replace($text, '\\\r?\n', '')
                if ($text -match '(?im)^\s*#\s*(?:using|embed)\b') { throw "Unsupported native file-loading directive: $path" }
                $includeMatches = @([regex]::Matches($text, '(?m)^\s*#\s*(?:include|import)\s+([^\r\n]+)')) + @([regex]::Matches($text, '__has_include\s*\(\s*([^\r\n)]+)\)'))
                foreach ($match in $includeMatches) {
                    $operand = $match.Groups[1].Value.Trim()
                    if ($operand -notmatch '^[<"]([^>"]+)[>"]') { throw "Macro native includes are unsupported: $path" }
                    $include = $Matches[1]
                    $extension = [IO.Path]::GetExtension($include)
                    if ($extension -and $extension -notin @('.h','.hpp','.inl','.hlsl','.inc')) { throw "Unsupported native include extension: $path" }
                    if ([IO.Path]::IsPathRooted($include)) { throw "Absolute native includes are unsupported: $path" }
                    if ($include -match '(^|[\\/])\.\.([\\/]|$)') {
                        $full = Resolve-PerformanceInputPath $include $entry.DirectoryName
                        $relativeInclude = if (Test-PerformancePathWithin $full $rootPath) { $full.Substring($rootPath.Length).TrimStart('\','/').Replace('\','/') } else { '' }
                        if (-not $owned.Contains($relativeInclude) -or -not @($nativeDirectories | Where-Object { Test-PerformancePathWithin $full (Join-Path $rootPath $_) }).Count) { throw "Escaping native includes are unsupported: $path" }
                    }
                }
                if ($text -match '(?im)^\s*#\s*pragma\s+(?:include_alias|comment\s*\(\s*linker\b)' -or
                    $text -match '(?im)^\s*#\s*pragma\s+comment\s*\(\s*lib\s*,\s*"[^"\r\n]*[\\/:@]') { throw "Path-bearing native pragma inputs are unsupported: $path" }
            }
        }
    }
    $resolved = [Collections.Generic.List[string]]::new()
    foreach ($kind in @('ClCompile','FxCompile')) {
        foreach ($item in @($Items[$kind])) {
            if (-not $item.Contains('FullPath')) { throw "Resolved $kind input omits FullPath." }
            $full = Resolve-PerformanceInputPath $item['FullPath'] $projectDirectory
            if ($full -ine (Resolve-PerformanceInputPath $item['Identity'] $projectDirectory) -or -not $inputFiles.ContainsKey($full)) { throw "Resolved $kind input is missing or outside the owned native closure: $full" }
            $resolved.Add("$kind $(Normalize-PerformanceBuildPath $full $rootPath) $($inputFiles[$full])")
        }
    }
    $assertDirectory = {
        param([string] $Value, [string] $Kind)
        $full = Resolve-PerformanceInputPath $Value $projectDirectory
        $known = if ($Kind -eq 'include') { $headers } else { $libraries }
        if ($known -icontains $full) { return }
        $dependency = Join-Path $dependencyRoot $(if ($Kind -eq 'include') { 'include' } else { 'lib' })
        if (Test-PerformancePathWithin $full $dependency) { return }
        if ($Kind -eq 'include') {
            if ($full -ieq [IO.Path]::GetFullPath($intDir)) { return }
            foreach ($relative in $nativeDirectories) { if (Test-PerformancePathWithin $full (Join-Path $rootPath $relative)) { return } }
        } elseif ($full -ieq [IO.Path]::GetFullPath($outDir)) { return }
        throw "Unsupported external or ignored $Kind search path: $Value"
    }
    foreach ($name in @('IncludePath','ExternalIncludePath','LibraryPath','ReferencePath')) {
        foreach ($path in ([string]$Properties[$name]).Split(';') | Where-Object { $_.Trim() }) { & $assertDirectory $path $(if ($name -in @('LibraryPath','ReferencePath')) { 'library' } else { 'include' }) }
    }
    foreach ($name in @('INCLUDE','LIB','LIBPATH')) {
        foreach ($path in ([string][Environment]::GetEnvironmentVariable($name)).Split(';') | Where-Object { $_.Trim() }) { & $assertDirectory $path $(if ($name -eq 'INCLUDE') { 'include' } else { 'library' }) }
    }
    foreach ($name in @('CL','_CL_')) { Assert-PerformanceInputOptions ([Environment]::GetEnvironmentVariable($name)) }
    Assert-PerformanceInputOptions ([string]$Properties['LibAdditionalOptions'])
    foreach ($kind in @('ClCompile','FxCompile','Link','Lib')) {
        foreach ($item in @($Items[$kind])) {
            Assert-PerformanceInputOptions ([string]$item['AdditionalOptions'])
            foreach ($name in @('AdditionalIncludeDirectories','ExternalIncludeDirectories','AdditionalUsingDirectories','AdditionalLibraryDirectories')) {
                foreach ($path in ([string]$item[$name]).Split(';') | Where-Object { $_.Trim() }) { & $assertDirectory $path $(if ($name -eq 'AdditionalLibraryDirectories') { 'library' } else { 'include' }) }
            }
            foreach ($name in @('ForcedIncludeFiles','ForcedUsingFiles','ModuleDefinitionFile')) {
                foreach ($path in ([string]$item[$name]).Split(';') | Where-Object { $_.Trim() }) {
                    $full = Resolve-PerformanceInputPath $path $projectDirectory
                    if (-not $inputFiles.ContainsKey($full) -and -not (Test-PerformancePathWithin $full (Join-Path $dependencyRoot 'include'))) { throw "Unsupported forced native input: $path" }
                    if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Missing forced native input: $path" }
                }
            }
            foreach ($path in ([string]$item['AdditionalDependencies']).Split(';') | Where-Object { $_.Trim() }) {
                if ($path -notmatch '^[^\\/:@]+\.lib$') {
                    $full = Resolve-PerformanceInputPath $path $projectDirectory
                    if ($full -ine (Join-Path $outDir 'DxUi.lib') -and -not (Test-PerformancePathWithin $full (Join-Path $dependencyRoot 'lib'))) { throw "Unsupported explicit native library input: $path" }
                } elseif ($path -ine 'DxUi.lib' -and -not @($libraries + @((Join-Path $dependencyRoot 'lib')) | Where-Object { Test-Path -LiteralPath (Join-Path $_ $path) -PathType Leaf }).Count) {
                    throw "The native library input is not in the selected attested tool/dependency roots: $path"
                }
            }
            if ($kind -eq 'ClCompile' -and (($item['PrecompiledHeader'] -and $item['PrecompiledHeader'] -cne 'NotUsing') -or $item['BuildStlModules'] -eq 'true' -or $item['GenerateModuleDependencies'] -eq 'true' -or $item['TranslateIncludes'] -eq 'true')) {
                throw 'Precompiled headers and C++ module inputs are unsupported by the native performance profile.'
            }
            if ($kind -eq 'ClCompile' -and (Resolve-PerformanceInputPath $item['ObjectFileName'] $projectDirectory) -ine [IO.Path]::GetFullPath($intDir)) { throw 'Custom compiler object output paths are unsupported.' }
            foreach ($name in @('TrackerLogDirectory','SourceDependenciesFile','ModuleOutputFile','PrecompiledHeaderOutputFile','AssemblerListingLocation','BrowseInformationFile','ProgramDataBaseFileName')) {
                if ($item[$name]) {
                    $full = Resolve-PerformanceInputPath $item[$name] $projectDirectory
                    if (-not (Test-PerformancePathWithin $full $intDir) -and -not (Test-PerformancePathWithin $full $outDir)) { throw "Unsupported native output/cache path: $name" }
                }
            }
            if ($kind -in @('Link','Lib')) {
                $outputName = if ($Role -eq 'library') { 'DxUi.lib' } else { 'DxUi.EmbeddedTests.exe' }
                if ((Resolve-PerformanceInputPath $item['OutputFile'] $projectDirectory) -ine (Join-Path $outDir $outputName)) { throw 'Custom linker/librarian output paths are unsupported.' }
            }
            if ($kind -eq 'FxCompile') {
                $suffix = if ($item['ShaderType'] -eq 'Vertex') { 'VS' } else { 'PS' }
                foreach ($output in @(@('HeaderFileOutput',"DxUiComposite$suffix.h"), @('ObjectFileOutput',"DxUiComposite$suffix.cso"))) {
                    if ((Resolve-PerformanceInputPath $item[$output[0]] $projectDirectory) -ine (Join-Path $intDir $output[1])) { throw 'Custom shader output paths are unsupported.' }
                }
            }
        }
    }
    # IntDir is an include path for two derived shader headers only. Other headers there would be ignored source inputs.
    if (Test-Path -LiteralPath $intDir -PathType Container) {
        if ((Get-Item -LiteralPath $intDir).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse native intermediate roots are unsupported.' }
        $failureMarker = Join-Path $intDir "$projectName.tlog/unsuccessfulbuild"
        foreach ($file in Get-ChildItem -LiteralPath $intDir -Recurse -File -Force) {
            $shaderOutputs = @('DxUiCompositeVS.h','DxUiCompositePS.h','DxUiCompositeVS.cso','DxUiCompositePS.cso' | ForEach-Object { Join-Path $intDir $_ })
            # MSBuild leaves this status marker after a failed compilation. It permits retry, never build reuse or a
            # blanket extensionless-input allowance; the incremental receipt still binds its presence and bytes.
            $isFailureMarker = $file.FullName -ieq $failureMarker -and -not ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)
            if ($file.FullName -notin $shaderOutputs -and -not $isFailureMarker -and $file.Extension -notin @('.obj','.tlog','.log','.lastbuildstate','.recipe','.pdb','.idb','.ilk','.ipdb','.iobj')) { throw "Unsupported ignored input/output in the native intermediate directory: $($file.FullName)" }
        }
    }
    if (Test-Path -LiteralPath $outDir -PathType Container) {
        if (@(Get-ChildItem -LiteralPath $outDir -File -Filter '*.lib' | Where-Object Name -ine 'DxUi.lib').Count) { throw 'Foreign ignored library inputs in the native output directory are unsupported.' }
    }
    return [ordered]@{ identity=(Get-ScopedDigest ("dxui-native-inputs-v2`n" + (($rows.ToArray() + $resolved.ToArray() | Sort-Object -CaseSensitive) -join "`n")));
        toolInputDirectories=@($headers + $libraries | Sort-Object -Unique) }
}

function Get-PerformanceResolvedToolPaths {
    [CmdletBinding()]
    param([Parameter(Mandatory)][Collections.IDictionary] $Properties, [Parameter(Mandatory)][string] $Platform)
    $architectures = @{ Native32Bit='x86'; Native64Bit='x64'; NativeARM64='arm64' }
    $hostArchitecture = [string]$Properties['PreferredToolArchitecture']
    if ($hostArchitecture -notin @('x86','x64','arm64') -or $architectures[$Properties['VCToolArchitecture']] -cne $hostArchitecture) {
        throw 'MSBuild returned an unsupported or inconsistent compiler host architecture.'
    }
    if ([string]$Properties['PlatformToolset'] -notmatch '^v[0-9]+$') { throw 'Only the resolved MSVC toolset is supported by performance attestation.' }
    $compilerDirectory = Join-Path $Properties['VCToolsInstallDir'] "bin/Host$hostArchitecture/$Platform"
    $paths = [ordered]@{}
    foreach ($entry in @(@('CL','cl.exe'), @('Link','link.exe'), @('Lib','lib.exe'))) {
        $prefix = $entry[0]
        $toolExe = [string]$Properties["${prefix}ToolExe"]
        $toolPath = [string]$Properties["${prefix}ToolPath"]
        if (($toolExe -and $toolExe -ine $entry[1]) -or ($toolPath -and [IO.Path]::GetFullPath($toolPath).TrimEnd('\','/') -ine [IO.Path]::GetFullPath($compilerDirectory).TrimEnd('\','/'))) {
            throw "A custom $prefix executable/path is unsupported by performance attestation."
        }
        $architecture = [string]$Properties["${prefix}ToolArchitecture"]
        if ($architecture -and $architectures[$architecture] -cne $hostArchitecture) { throw "A custom $prefix tool architecture is unsupported." }
        $paths[$prefix] = Join-Path $compilerDirectory $entry[1]
    }
    $shaderArchitecture = [string]$Properties['FxCompileToolArchitecture']
    if (-not $shaderArchitecture) { $shaderArchitecture = [string]$Properties['WindowsSDKToolArchitecture'] }
    if (-not $architectures.ContainsKey($shaderArchitecture)) { throw 'MSBuild returned an unsupported FXC host architecture.' }
    $shaderDirectory = [string]$Properties['FXCToolPath']
    if (-not $shaderDirectory) {
        $sdk = ([string]$Properties['WindowsTargetPlatformVersion']).TrimEnd('\','/')
        $shaderDirectory = Join-Path $Properties['WindowsSdkDir'] "bin/$sdk/$($architectures[$shaderArchitecture])"
    }
    if ($Properties['MultiProcFXC'] -eq 'true' -or $Properties['FxCompileToolExe']) { throw 'Custom or parallel FXC tool selection is unsupported by performance attestation.' }
    $paths['FXC'] = Join-Path $shaderDirectory 'fxc.exe'
    foreach ($path in $paths.Values) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Resolved toolchain file is missing: $path" } }
    return $paths
}

function Get-PerformanceToolIdentities {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $Root,
        [Parameter(Mandatory)][string] $Platform,
        [Parameter(Mandatory)][string] $Configuration,
        [ValidateSet('Full','PlatformOnly','Build')][string] $IdentityScope = 'Full'
    )
    try {
        $rootPath = [IO.Path]::GetFullPath($Root)
        $scopeName = switch ($IdentityScope) { 'Full' { 'full' }; 'PlatformOnly' { 'platform-only' }; 'Build' { 'build-only' } }
        $hashText = {
            param([string] $Text)
            $sha = [Security.Cryptography.SHA256]::Create()
            try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text))) }
            finally { $sha.Dispose() }
        }
        $hashFiles = {
            param([string[]] $Paths)
            $lines = foreach ($relative in $Paths) {
                $file = Join-Path $rootPath $relative
                if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Cannot attest missing input: $relative" }
                "$relative $((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash)"
            }
            & $hashText ($lines -join "`n")
        }
        $hashTree = {
            param([string[]] $Paths, [switch] $AllowEmpty, [switch] $AllFiles, [switch] $DllOnly)
            $files = [Collections.Generic.List[string]]::new()
            foreach ($path in $Paths) {
                if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Cannot attest missing toolchain/dependency directory: $path" }
                if ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse tool/dependency roots are unsupported: $path" }
                foreach ($file in Get-ChildItem -LiteralPath $path -Recurse -Force -ErrorAction Stop) {
                    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse tool/dependency inputs are unsupported: $($file.FullName)" }
                    if ($file.PSIsContainer) { continue }
                    $name = if ($file.FullName.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) { $file.FullName.Substring($rootPath.Length).TrimStart('\','/') } else { $file.FullName }
                    if ($AllFiles -or ($DllOnly -and $name -match '(?i)\.dll$') -or (-not $DllOnly -and -not $AllFiles -and ($name -match '\.(h|hpp|inl|idl|lib|dll)$' -or $file.Name -match '^(vcpkg_abi_info\.txt|vcpkg\.spdx\.json)$'))) {
                        $files.Add("$name $((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)")
                    }
                }
            }
            if (-not $AllowEmpty -and $files.Count -eq 0) { throw "The attested file closure is empty: $($Paths -join ', ')" }
            & $hashText (($files.ToArray() | Sort-Object) -join "`n")
        }

        $harness = ''
        if ($IdentityScope -eq 'Full') {
            $harness = & $hashFiles -Paths @('performance.ps1','performance-paired.ps1','build.ps1','vcpkg-install.ps1',
                'Tools/Compare-Performance.ps1','Tools/PerformanceComparison.psm1','Tools/PairedRun.psm1','Tools/BenchmarkGate.psm1','Tools/PerformancePolicy.psm1','Tools/PerformanceAcceptancePolicy.v1.json',
                'Tools/VisualStudio.psm1','Tools/VcpkgTriplet.psm1','Tools/ScopedTesting.psm1','Tools/Validation.psm1','Tools/Changelog.psm1','Tools/Modules/Documentation/Changelog.psm1','vcpkg.json','vcpkg-tool.json')
        }
        Import-Module (Join-Path $rootPath 'Tools/VisualStudio.psm1') -Force
        $installation = Get-DxUiVisualStudioInstallation
        $msbuildPath = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
        if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) { throw 'MSBuild identity is unavailable.' }
        $buildOutputRoot = [IO.Path]::GetFullPath((Join-Path $rootPath '.build')).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
        $toolsetProps = Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'
        if (-not (Test-Path -LiteralPath $toolsetProps -PathType Leaf)) { throw "Resolved toolset selection is missing: $toolsetProps" }
        $toolchainLines = @('dxui-performance-toolchain-v2', "MSBuild=$((Get-FileHash -LiteralPath $msbuildPath -Algorithm SHA256).Hash)",
            "MSBuildFiles=$(& $hashTree -Paths @((Split-Path -Parent $msbuildPath)) -AllFiles)",
            "ToolsetSelection=$((Get-FileHash -LiteralPath $toolsetProps -Algorithm SHA256).Hash)")
        $resolvedProjects = [ordered]@{}
        $closureCache = @{}
        foreach ($entry in @(@('library','src/DxUi.vcxproj'), @('benchmark','Tests/Embedded/DxUi.EmbeddedTests.vcxproj'))) {
            $role = $entry[0]
            $evaluation = Get-PerformanceProjectEvaluation -MSBuildPath $msbuildPath -ProjectPath (Join-Path $rootPath $entry[1]) -Configuration $Configuration -Platform $Platform -OutputRoot $buildOutputRoot -Root $rootPath -Role $role
            $properties = $evaluation['properties']
            $profile = $evaluation['profile']
            if ([string]::IsNullOrWhiteSpace([string]$evaluation['solutionIdentity'])) { throw 'The resolved solution configuration identity is missing.' }
            if (-not $evaluation.Contains('inputs') -or [string]::IsNullOrWhiteSpace([string]$evaluation['inputs']['identity'])) { throw 'The resolved native source byte identity is missing.' }
            $resolvedProjects[$role] = [ordered]@{ path=$entry[1]; projectSha256=$evaluation['projectSha256']; solutionIdentity=$evaluation['solutionIdentity']; expandedProjectIdentity=$profile['expandedProjectIdentity']; resolvedItemsIdentity=$profile['resolvedItemsIdentity']; nativeInputIdentity=$evaluation['inputs']['identity'] }
            $toolchainLines += "$role.Profile=$($profile['profileIdentity'])"
            $toolPaths = Get-PerformanceResolvedToolPaths -Properties $properties -Platform $Platform
            foreach ($name in $toolPaths.Keys) { $toolchainLines += "$role.$name=$((Get-FileHash -LiteralPath $toolPaths[$name] -Algorithm SHA256).Hash)" }
            $sdk = ([string]$properties['WindowsTargetPlatformVersion']).TrimEnd('\','/')
            $sdkRoot = ([string]$properties['WindowsSdkDir']).TrimEnd('\','/')
            $closures = [ordered]@{
                MSVC=@((Join-Path $properties['VCToolsInstallDir'] 'include'), (Join-Path $properties['VCToolsInstallDir'] "lib/$Platform"), (Split-Path -Parent $toolPaths['CL']))
                SDK=@((Join-Path $sdkRoot "Include/$sdk/um"), (Join-Path $sdkRoot "Include/$sdk/shared"), (Join-Path $sdkRoot "Include/$sdk/ucrt"),
                    (Join-Path $sdkRoot "Lib/$sdk/um/$Platform"), (Join-Path $sdkRoot "Lib/$sdk/ucrt/$Platform"))
                # Include tool executables and task/helper DLLs: the resolved fxc.exe alone is not its loaded code closure.
                SDKTools=@((Split-Path -Parent $toolPaths['FXC']))
                VCTargets=@([string]$properties['VCTargetsPath'])
            }
            foreach ($name in $closures.Keys) {
                $cacheKey = "$name|$($closures[$name] -join '|')"
                if (-not $closureCache.ContainsKey($cacheKey)) { $closureCache[$cacheKey] = & $hashTree -Paths $closures[$name] -AllFiles }
                $toolchainLines += "$role.${name}Files=$($closureCache[$cacheKey])"
            }
            foreach ($directory in $evaluation['inputs']['toolInputDirectories']) {
                $cacheKey = "Native|$directory"
                if (-not $closureCache.ContainsKey($cacheKey)) {
                    # Some selected default optional paths are absent in an installation. Bind that absence as well;
                    # adding the directory later changes toolchain identity rather than introducing invisible inputs.
                    $closureCache[$cacheKey] = if (Test-Path -LiteralPath $directory -PathType Container) { & $hashTree -Paths @($directory) -AllFiles -AllowEmpty } else { 'missing' }
                }
                $toolchainLines += "$role.Native.$directory=$($closureCache[$cacheKey])"
            }
        }
        $resolvedBuildIdentity = Get-ScopedDigest ("dxui-resolved-projects-v2`n" + ($resolvedProjects | ConvertTo-Json -Depth 12 -Compress))
        # cl.exe also consumes CL/_CL_ independently of evaluated project metadata. Hash values without publishing them.
        foreach ($name in @('CL','_CL_','INCLUDE','LIB','LIBPATH','PATH','VCToolsVersion','WindowsSDKVersion','PreferredToolArchitecture')) {
            $value = [Environment]::GetEnvironmentVariable($name)
            $valueHash = if ($null -eq $value) { 'unset' } else { & $hashText $value }
            $toolchainLines += "Environment.$name=$valueHash"
        }
        $toolchain = & $hashText ($toolchainLines -join "`n")

        # Build guards run before output/runtime directories exist. Their limited scope binds both compiled projects,
        # CL/FXC/link/lib profiles and installed tool inputs, and cannot qualify a full measurement receipt.
        if ($IdentityScope -eq 'Build') {
            return [ordered]@{ buildIdentityVersion=2; resolvedBuildIdentity=$resolvedBuildIdentity; identityStatus='build-only'; identityScope=$scopeName; harnessIdentity=''; toolchainIdentity=$toolchain; dependencyIdentity=''; environmentIdentity='' }
        }

        $triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
        $installRoot = Join-Path $rootPath ".build/vcpkg_installed/$Platform"
        $statusPath = Join-Path $installRoot 'vcpkg/status'
        if (-not (Test-Path -LiteralPath $statusPath -PathType Leaf)) { throw "Dependency package status is unavailable: $statusPath" }
        $packageInfo = @(Get-ChildItem -LiteralPath (Join-Path $installRoot 'vcpkg/info') -File -Filter '*.list' -ErrorAction Stop)
        if (-not $packageInfo.Count) { throw "Dependency package file inventory is unavailable under $installRoot/vcpkg/info" }
        $abiFiles = @(Get-ChildItem -LiteralPath (Join-Path $installRoot "$triplet/share") -File -Recurse -ErrorAction Stop | Where-Object Name -in @('vcpkg_abi_info.txt','vcpkg.spdx.json'))
        $dependencyFiles = @('vcpkg.json','vcpkg-tool.json',".build/vcpkg_installed/$Platform/vcpkg/status") + @($packageInfo | ForEach-Object { $_.FullName.Substring($rootPath.Length).TrimStart('\','/') }) + @($abiFiles | ForEach-Object { $_.FullName.Substring($rootPath.Length).TrimStart('\','/') })
        $dependencyBase = & $hashFiles -Paths $dependencyFiles
        $installedTreeIdentity = & $hashTree -Paths @((Join-Path $installRoot $triplet)) -AllFiles
        $installedBin = Join-Path $installRoot "$triplet/bin"
        $outputRoot = Join-Path $rootPath ".build/$Platform/$Configuration"
        if (-not (Test-Path -LiteralPath $outputRoot -PathType Container)) { throw "Runtime output directory is unavailable: $outputRoot" }
        $declaredRuntimeDll = @($packageInfo | ForEach-Object { Get-Content -LiteralPath $_.FullName } | Where-Object { $_ -match '(?i)(^|/)bin/[^/]+\.dll$' }).Count -gt 0
        if ((-not (Test-Path -LiteralPath $installedBin -PathType Container)) -and $declaredRuntimeDll) { throw "Installed runtime DLL directory is unavailable: $installedBin" }
        $runtimeDirs = @($outputRoot)
        if (Test-Path -LiteralPath $installedBin -PathType Container) { $runtimeDirs += $installedBin }
        $runtimeIdentity = & $hashTree -Paths $runtimeDirs -AllowEmpty -DllOnly
        $dependency = & $hashText "$triplet;$dependencyBase;installedTree=$installedTreeIdentity;runtimeDllClosure=$runtimeIdentity"

        $gpu = @(Get-CimInstance Win32_VideoController | Sort-Object PNPDeviceID | ForEach-Object { "$($_.PNPDeviceID)|$($_.DriverVersion)|$($_.DriverDate)" })
        if (-not $gpu.Count -or @($gpu | Where-Object { $parts=$_.Split('|'); [string]::IsNullOrWhiteSpace($parts[0]) -or [string]::IsNullOrWhiteSpace($parts[1]) }).Count) { throw 'GPU driver identity is unavailable.' }
        if (-not ('DxUiDpiProbe' -as [type])) { Add-Type -Name DxUiDpiProbe -Namespace DxUi -MemberDefinition '[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern uint GetDpiForSystem();' -ErrorAction Stop }
        $dpi = [DxUi.DxUiDpiProbe]::GetDpiForSystem()
        if ($dpi -le 0) { throw 'System DPI identity is unavailable.' }
        $cpu = (Get-CimInstance Win32_Processor | ForEach-Object Name) -join ';'
        if ([string]::IsNullOrWhiteSpace($cpu)) { throw 'CPU identity is unavailable.' }
        $warpPath = Join-Path $env:SystemRoot 'System32/d3d10warp.dll'
        if (-not (Test-Path -LiteralPath $warpPath -PathType Leaf)) { throw 'WARP binary identity is unavailable.' }
        $warpSha = (Get-FileHash -LiteralPath $warpPath -Algorithm SHA256).Hash
        $environment = & $hashText (@("OS=$([Runtime.InteropServices.RuntimeInformation]::OSDescription)","Architecture=$([Runtime.InteropServices.RuntimeInformation]::OSArchitecture)","Machine=$env:COMPUTERNAME","CPU=$cpu","GPU=$($gpu -join ';')","SystemDpi=$dpi","WARP=$warpSha",
            "ScopedEnvironment=$(Get-ScopedEnvironmentIdentity)") -join "`n")
        $status = if ($IdentityScope -eq 'Full') { 'verifiable' } else { 'platform-only' }
        return [ordered]@{ buildIdentityVersion=2; resolvedBuildIdentity=$resolvedBuildIdentity; identityStatus=$status; identityScope=$scopeName; harnessIdentity=$harness; toolchainIdentity=$toolchain; dependencyIdentity=$dependency; environmentIdentity=$environment }
    }
    catch {
        $scopeName = switch ($IdentityScope) { 'Full' { 'full' }; 'PlatformOnly' { 'platform-only' }; 'Build' { 'build-only' } }
        return [ordered]@{ identityStatus='identity-unverifiable'; identityScope=$scopeName; identityError=$_.Exception.Message; harnessIdentity=''; toolchainIdentity=''; dependencyIdentity=''; environmentIdentity='' }
    }
}

function Get-PerformanceBuildAttestationPath {
    param([string] $Root, [string] $Platform, [string] $Configuration)
    return Join-Path $Root ".build/reports/performance-build/$Platform-$($Configuration -replace ' ','')/build.json"
}

function Get-PerformanceIncrementalAttestationPath {
    param([string] $Root, [string] $Platform, [string] $Configuration)
    return Join-Path (Split-Path -Parent (Get-PerformanceBuildAttestationPath $Root $Platform $Configuration)) 'incremental.json'
}

function Get-PerformanceIntermediateIdentity {
    param([string] $Root, [ValidateSet('x64','ARM64')][string] $Platform,
        [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration)
    $directory = [IO.Path]::GetFullPath((Join-Path $Root ".build/Intermediate/$Platform/$Configuration"))
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw 'The incremental build profile is missing.' }
    if ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse incremental profile roots are unsupported.' }
    # A receipt for only final artifacts cannot establish where cached objects/generated headers came from. Bind the
    # complete profile (all solution projects, all extensions, inventories and bytes), not merely timestamps or .obj files.
    foreach ($name in @('DxUi','DxUi.EmbeddedTests')) {
        $project = Join-Path $directory $name
        if (-not (Test-Path -LiteralPath $project -PathType Container) -or
                -not @(Get-ChildItem -LiteralPath $project -Recurse -File -Filter '*.obj' -ErrorAction Stop).Count -or
                -not @(Get-ChildItem -LiteralPath $project -Recurse -File -Filter '*.tlog' -ErrorAction Stop).Count) { throw "The incremental $name object/tracking closure is incomplete." }
    }
    foreach ($name in @('DxUiCompositeVS.h','DxUiCompositeVS.cso','DxUiCompositePS.h','DxUiCompositePS.cso')) {
        if (-not (Test-Path -LiteralPath (Join-Path $directory "DxUi/$name") -PathType Leaf)) { throw "The incremental shader closure is incomplete: $name" }
    }
    $rows = [Collections.Generic.List[string]]::new()
    foreach ($entry in Get-ChildItem -LiteralPath $directory -Recurse -Force -ErrorAction Stop) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse points are unsupported in the incremental build profile.' }
        $relative = $entry.FullName.Substring($directory.Length).TrimStart('\','/').Replace('\','/')
        if ($entry.PSIsContainer) { $rows.Add("directory $relative") }
        else { $rows.Add("file $relative $((Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash)") }
    }
    return Get-ScopedDigest ("dxui-performance-intermediates-v1`n" + (($rows.ToArray() | Sort-Object -CaseSensitive) -join "`n"))
}

function Get-PerformanceIncrementalAttestationIdentity([string] $BuildIdentity, [string] $IntermediateIdentity) {
    return Get-ScopedDigest (@('dxui-performance-incremental-v1',$BuildIdentity,$IntermediateIdentity) -join "`n")
}

function Get-PerformanceBuildState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration)
    $tools = Get-PerformanceToolIdentities -Root $Root -Platform $Platform -Configuration $Configuration -IdentityScope Build
    if ($tools.identityStatus -cne 'build-only' -or $tools['buildIdentityVersion'] -ne 2 -or [string]::IsNullOrWhiteSpace([string]$tools.toolchainIdentity) -or
            [string]::IsNullOrWhiteSpace([string]$tools['resolvedBuildIdentity'])) {
        throw "Cannot attest benchmark build toolchain: $($tools['identityError'])"
    }
    return [ordered]@{ buildIdentityVersion=2; resolvedBuildIdentity=$tools['resolvedBuildIdentity'];
        buildInputIdentity=(Get-PerformanceBuildInputIdentity -Root $Root -Platform $Platform -ResolvedBuildIdentity $tools['resolvedBuildIdentity']); toolchainIdentity=$tools.toolchainIdentity }
}

function Get-PerformanceBuildInputIdentity {
    param([string] $Root, [string] $Platform, [string] $ResolvedBuildIdentity)
    if ([string]::IsNullOrWhiteSpace($ResolvedBuildIdentity)) { throw 'The resolved build input identity is missing.' }
    return Get-ScopedDigest (@('dxui-performance-input-v2', (Get-ScopedBuildInputIdentity $Root $Platform), $ResolvedBuildIdentity) -join "`n")
}

function Start-PerformanceBuildAttestation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration)
    $path = Get-PerformanceBuildAttestationPath $Root $Platform $Configuration
    $incrementalPath = Get-PerformanceIncrementalAttestationPath $Root $Platform $Configuration
    try {
        $state = Get-PerformanceBuildState -Root $Root -Platform $Platform -Configuration $Configuration
        $rebuildRequired = $true
        $executable = Join-Path $Root ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
        if ((Test-Path -LiteralPath $path -PathType Leaf) -and (Test-Path -LiteralPath $executable -PathType Leaf)) {
            $artifact = Get-ScopedArtifactIdentity $Root $Platform $Configuration
            $identity = Get-PerformanceBuildAttestationIdentity -State $state -ArtifactIdentity $artifact
            if (Test-ScopedReceipt -Path $path -Identity $identity) {
                try {
                    $intermediate = Get-PerformanceIntermediateIdentity $Root $Platform $Configuration
                    $incremental = Get-PerformanceIncrementalAttestationIdentity $identity $intermediate
                    $rebuildRequired = -not (Test-ScopedReceipt -Path $incrementalPath -Identity $incremental)
                } catch {
                    # Missing/incomplete/altered cache is repairable with Rebuild. Final-output SkipBuild stays independent
                    # of disposable intermediates and still uses the separate v2 build receipt.
                    $rebuildRequired = $true
                }
            }
        }
        # Build requires matching final outputs AND the exact intermediate closure from a successful guarded build.
        $state['rebuildRequired'] = $rebuildRequired
        return $state
    } finally {
        # Revoke a previous pass before any build attempt, including a pre-compilation attestation failure.
        if (Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path }
        if (Test-Path -LiteralPath $incrementalPath -PathType Leaf) { Remove-Item -LiteralPath $incrementalPath }
    }
}

function Get-PerformanceBuildAttestationIdentity {
    param([System.Collections.IDictionary] $State, [string] $ArtifactIdentity)
    if (-not $State.Contains('buildIdentityVersion') -or $State['buildIdentityVersion'] -ne 2) { throw 'A v2 benchmark build identity is required; v1 receipts need a fresh build.' }
    foreach ($name in @('resolvedBuildIdentity','buildInputIdentity','toolchainIdentity')) {
        if (-not $State.Contains($name) -or [string]::IsNullOrWhiteSpace([string]$State[$name])) { throw "The build attestation has no $name." }
    }
    if ([string]::IsNullOrWhiteSpace($ArtifactIdentity)) { throw 'The build attestation has no artifact identity.' }
    return Get-ScopedDigest (@('dxui-performance-build-v2', $State['buildIdentityVersion'], $State['resolvedBuildIdentity'], $State['buildInputIdentity'], $State['toolchainIdentity'], $ArtifactIdentity) -join "`n")
}

function Complete-PerformanceBuildAttestation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration,
        [Parameter(Mandatory)][System.Collections.IDictionary] $Before)
    $path = Get-PerformanceBuildAttestationPath $Root $Platform $Configuration
    $incrementalPath = Get-PerformanceIncrementalAttestationPath $Root $Platform $Configuration
    try {
        $after = Get-PerformanceBuildState -Root $Root -Platform $Platform -Configuration $Configuration
        foreach ($key in @('buildIdentityVersion','resolvedBuildIdentity','buildInputIdentity','toolchainIdentity')) {
            if ([string]$Before[$key] -cne [string]$after[$key]) { throw "Benchmark $key changed during build; no build attestation was published." }
        }
        $executable = Join-Path $Root ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Benchmark executable is missing after build: $executable" }
        $artifact = Get-ScopedArtifactIdentity $Root $Platform $Configuration
        $identity = Get-PerformanceBuildAttestationIdentity -State $after -ArtifactIdentity $artifact
        $intermediate = Get-PerformanceIntermediateIdentity $Root $Platform $Configuration
        Write-ScopedReceipt -Path $path -Identity $identity
        Write-ScopedReceipt -Path $incrementalPath -Identity (Get-PerformanceIncrementalAttestationIdentity $identity $intermediate)
    } catch {
        # A nested modern build may have published a receipt before the outer, legacy-compatible guard notices drift.
        if (Test-Path -LiteralPath $path -PathType Leaf) { Remove-Item -LiteralPath $path }
        if (Test-Path -LiteralPath $incrementalPath -PathType Leaf) { Remove-Item -LiteralPath $incrementalPath }
        throw
    }
}

function Assert-PerformanceBuildAttestation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration,
        [System.Collections.IDictionary] $State)
    if ($null -eq $State) {
        $State = Get-PerformanceBuildState -Root $Root -Platform $Platform -Configuration $Configuration
        $State['buildArtifactIdentity'] = Get-ScopedArtifactIdentity $Root $Platform $Configuration
    }
    $identity = Get-PerformanceBuildAttestationIdentity -State $State -ArtifactIdentity ([string]$State['buildArtifactIdentity'])
    $path = Get-PerformanceBuildAttestationPath $Root $Platform $Configuration
    if (-not (Test-ScopedReceipt -Path $path -Identity $identity)) {
        throw 'SkipBuild refused: benchmark source, dependencies, resolved toolchain or artifacts changed, or no attested build exists. Run build.ps1 or performance.ps1 without -SkipBuild.'
    }
}

function Get-PerformanceMeasurementState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $Configuration)
    $tools = Get-PerformanceToolIdentities -Root $Root -Platform $Platform -Configuration $Configuration
    if ($tools.identityStatus -cne 'verifiable' -or $tools.identityScope -cne 'full' -or $tools['buildIdentityVersion'] -ne 2) { throw "Cannot attest benchmark measurement: $($tools['identityError'])" }
    $commit = @(& git -C $Root rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $commit.Count -ne 1) { throw 'Cannot identify benchmark source commit.' }
    $dirty = @(& git -C $Root status --porcelain)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify benchmark source status.' }
    $executable = Join-Path $Root ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
    $inputs = [ordered]@{}
    foreach ($path in Get-BenchmarkInputPaths) { $inputs[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path) -Algorithm SHA256).Hash }
    $fixture = ($inputs.GetEnumerator() | ForEach-Object { "$($_.Key) $($_.Value)" }) -join "`n"
    $warpPath = Join-Path $env:SystemRoot 'System32/d3d10warp.dll'
    $powerPolicy = ((& powercfg.exe /getactivescheme) -join ' ').Trim()
    if ($LASTEXITCODE -ne 0 -or -not $powerPolicy) { throw 'Cannot identify the active benchmark power policy.' }
    $state = [ordered]@{
        platform=$Platform; configuration=$Configuration; nativeArchitecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        machine=[Environment]::MachineName; cpu=(Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
        os=[Runtime.InteropServices.RuntimeInformation]::OSDescription; warpVersion=(Get-Item -LiteralPath $warpPath).VersionInfo.FileVersion
        warpSha256=(Get-FileHash -LiteralPath $warpPath -Algorithm SHA256).Hash; powerPolicy=$powerPolicy
        sourceCommit=$commit[0].Trim(); sourceDirty=[bool]$dirty.Count; sourceFingerprint=(Get-SourceFingerprint -Root $Root)
        executableSha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; benchmarkInputs=$inputs
        # Keep the historical receipt's uppercase SHA-256 and literal path/hash lines.
        benchmarkSha256=(Get-ScopedDigest $fixture).ToUpperInvariant()
        buildInputIdentity=(Get-PerformanceBuildInputIdentity -Root $Root -Platform $Platform -ResolvedBuildIdentity $tools['resolvedBuildIdentity']); buildArtifactIdentity=(Get-ScopedArtifactIdentity $Root $Platform $Configuration)
    }
    foreach ($key in $tools.Keys) { $state[$key] = $tools[$key] }
    $state['buildAttestationIdentity'] = Get-PerformanceBuildAttestationIdentity -State $state -ArtifactIdentity $state['buildArtifactIdentity']
    Assert-PerformanceBuildAttestation -Root $Root -Platform $Platform -Configuration $Configuration -State $state
    return $state
}

function Assert-PerformanceMeasurementState {
    [CmdletBinding()]
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Before, [Parameter(Mandatory)][System.Collections.IDictionary] $After)
    if ($Before.Count -ne $After.Count) { throw 'Benchmark measurement identity changed during execution.' }
    foreach ($key in $Before.Keys) {
        if (-not $After.Contains($key) -or (ConvertTo-Json -InputObject $Before[$key] -Depth 8 -Compress) -cne (ConvertTo-Json -InputObject $After[$key] -Depth 8 -Compress)) {
            throw "Benchmark $key changed during execution; the measured receipt cannot be qualified."
        }
    }
}

function Test-PerformanceReceiptIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Receipt)
    $required = @('sourceCommit','sourceFingerprint','executableSha256','benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity',
        'resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity',
        'fixture','renderer','width','height','dpi','controls','modelRows','framesPerRound','roundCount','platform','configuration','nativeArchitecture',
        'machine','cpu','os','compiler','powerPolicy','warpVersion','warpSha256')
    $missing = @($required | Where-Object { -not $Receipt.Contains($_) -or $null -eq $Receipt[$_] -or [string]::IsNullOrWhiteSpace([string]$Receipt[$_]) })
    if (-not $Receipt.Contains('identityStatus') -or $Receipt['identityStatus'] -cne 'verifiable') { $missing += 'identityStatus' }
    if (-not $Receipt.Contains('identityScope') -or $Receipt['identityScope'] -cne 'full') { $missing += 'identityScope' }
    if (-not $Receipt.Contains('buildIdentityVersion') -or $Receipt['buildIdentityVersion'] -ne 2) { $missing += 'buildIdentityVersion' }
    foreach ($name in @('resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity')) {
        if ($Receipt.Contains($name) -and [string]$Receipt[$name] -cnotmatch '^[A-Fa-f0-9]{64}$') { $missing += $name }
    }
    if ($missing.Count -eq 0 -and $Receipt['buildAttestationIdentity'] -cne (Get-PerformanceBuildAttestationIdentity -State $Receipt -ArtifactIdentity $Receipt['buildArtifactIdentity'])) {
        $missing += 'buildAttestationIdentity'
    }
    if ($Receipt.Contains('identityError') -and -not [string]::IsNullOrWhiteSpace([string]$Receipt['identityError'])) { $missing += "identityError:$($Receipt['identityError'])" }
    return [ordered]@{ verifiable=($missing.Count -eq 0); missing=@($missing | Select-Object -Unique) }
}

Export-ModuleMember -Function Get-TrustedPerformancePolicy, Test-PerformanceReceiptIdentity, Get-PerformanceToolIdentities, Get-ResolvedCompileItemsIdentity, Get-PreprocessedProjectIdentity,
    Get-PerformanceProjectProfile, Get-PerformanceResolvedToolPaths, Get-PerformanceBuildAttestationIdentity,
    Start-PerformanceBuildAttestation, Complete-PerformanceBuildAttestation, Assert-PerformanceBuildAttestation, Get-PerformanceMeasurementState, Assert-PerformanceMeasurementState
