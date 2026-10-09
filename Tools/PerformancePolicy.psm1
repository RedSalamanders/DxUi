# Trust checks for the migrated benchmark gate. A policy introduced by a candidate is never its own trust anchor.
Set-StrictMode -Version Latest

function Get-TrustedPerformancePolicy {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $BaselineCommit, [Parameter(Mandatory)][string] $RepositoryRoot, [Parameter(Mandatory)][string] $CandidatePolicyPath)
    $path = 'Tools/PerformanceAcceptancePolicy.v1.json'
    $baseText = @(& git -C $RepositoryRoot show "${BaselineCommit}:$path" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $baseText.Count -eq 0) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured base does not contain the versioned acceptance policy; the candidate cannot introduce its own trust anchor.'; policySha256=$null }
    }
    try { $policy = ($baseText -join "`n") | ConvertFrom-Json -AsHashtable }
    catch { return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The base policy is not valid JSON.'; policySha256=$null } }
    if ($policy.schemaVersion -ne 1 -or $policy.qualified -ne $true -or $policy.qualificationStatus -cne 'qualified') {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The base policy is not independently qualified; its verdict cannot establish a candidate pass.'; policySha256=$null }
    }
    if ([string]$policy.judgeVersion -notmatch '^dxui-[A-Za-z0-9-]+$' -or [string]$policy.approvedJudgeSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The qualified base policy does not bind an approved judge version and source hash.'; policySha256=$null }
    }
    $judgeLines = @(& git -C $RepositoryRoot show "${BaselineCommit}:Tools/PerformanceComparison.psm1" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $judgeLines.Count -eq 0) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The qualified base policy judge source cannot be recovered.'; policySha256=$null }
    }
    $judgeSource = (($judgeLines -join "`n") + "`n") -replace "`r`n", "`n"
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
    $assignmentLines = @(& git -C $RepositoryRoot show "${BaselineCommit}:Tools/PairedRun.psm1" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $assignmentLines.Count -eq 0) {
        return [ordered]@{ trusted=$false; status='policy-review-required'; reason='The measured-base assignment generator source cannot be recovered.'; policySha256=$null }
    }
    $assignmentSource = (($assignmentLines -join "`n") + "`n") -replace "`r`n", "`n"
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
        [Parameter(Mandatory)][string] $Root
    )
    if (-not (Test-Path -LiteralPath $MSBuildPath -PathType Leaf)) { throw "Resolved MSBuild executable is missing: $MSBuildPath" }
    if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Project to preprocess is missing: $ProjectPath" }
    $temporaryPath = Join-Path ([IO.Path]::GetTempPath()) ("DxUi-expanded-project-$([guid]::NewGuid().ToString('N')).xml")
    try {
        $arguments = @($ProjectPath, '/nologo', "/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$OutputRoot", "/pp:$temporaryPath")
        $output = @(& $MSBuildPath @arguments 2>&1)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) { throw "MSBuild project preprocessing failed with exit code $exitCode. $($output -join ' ')" }
        if (-not (Test-Path -LiteralPath $temporaryPath -PathType Leaf)) { throw 'MSBuild succeeded without producing preprocessed project output.' }
        return Get-PreprocessedProjectIdentity -Text ([IO.File]::ReadAllText($temporaryPath)) -Root $Root
    }
    finally {
        if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) { Remove-Item -LiteralPath $temporaryPath -Force }
    }
}

function Get-ResolvedCompileItemsIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][object[]] $Items, [Parameter(Mandatory)][string] $Root)
    $effectiveMetadata = @('AdditionalIncludeDirectories','AdditionalOptions','PreprocessorDefinitions','LanguageStandard','LanguageStandard_C',
        'RuntimeLibrary','WarningLevel','TreatWarningAsError','SDLCheck','ConformanceMode','DisableLanguageExtensions','ExceptionHandling',
        'Optimization','EnableASAN','DebugInformationFormat','ForcedIncludeFiles','CompileAs','CallingConvention','EnableEnhancedInstructionSet',
        'WholeProgramOptimization','BasicRuntimeChecks','BufferSecurityCheck','IntrinsicFunctions','TreatWChar_tAsBuiltInType',
        'FloatingPointModel','OmitDefaultLibName','RemoveUnreferencedCodeData','TreatExternalTemplatesAsInternal')
    $rows = [Collections.Generic.List[string]]::new()
    foreach ($item in $Items) {
        $values = [Collections.Generic.List[string]]::new()
        foreach ($name in @('Identity') + $effectiveMetadata) {
            $value = if ($item -is [System.Collections.IDictionary]) { $item[$name] } else { $item.$name }
            if ($null -ne $value) { $values.Add("$name=$(Normalize-PerformanceBuildPath ([string]$value) $Root)") }
        }
        if ($values.Count -le 1) { throw 'Resolved compile item has no effective compiler metadata.' }
        $rows.Add($values -join '|')
    }
    if (-not $rows.Count) { throw 'MSBuild resolved no compiler items.' }
    $canonical = @($rows.ToArray() | Sort-Object) -join "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))) }
    finally { $sha.Dispose() }
}

function Get-PerformanceToolIdentities {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $Root,
        [Parameter(Mandatory)][string] $Platform,
        [Parameter(Mandatory)][string] $Configuration,
        [ValidateSet('Full','PlatformOnly')][string] $IdentityScope = 'Full'
    )
    try {
        $rootPath = [IO.Path]::GetFullPath($Root)
        $scopeName = if ($IdentityScope -eq 'Full') { 'full' } else { 'platform-only' }
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
                foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse) {
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
                'Tools/VisualStudio.psm1','Tools/VcpkgTriplet.psm1','vcpkg.json','vcpkg-tool.json','src/DxUi.vcxproj')
        }
        Import-Module (Join-Path $rootPath 'Tools/VisualStudio.psm1') -Force
        $installation = Get-DxUiVisualStudioInstallation
        $msbuildPath = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
        if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf)) { throw 'MSBuild identity is unavailable.' }
        $projectPath = Join-Path $rootPath 'src/DxUi.vcxproj'
        $buildOutputRoot = [IO.Path]::GetFullPath((Join-Path $rootPath '.build')).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
        $projectArguments = @("/p:Configuration=$Configuration", "/p:Platform=$Platform", "/p:DxUiOutputRoot=$buildOutputRoot")
        $propertyNames = 'PlatformToolset,VCToolsInstallDir,VCToolsVersion,WindowsSdkDir,WindowsTargetPlatformVersion,PreferredToolArchitecture,DxUiOutputRoot'
        $propertiesJson = & $msbuildPath $projectPath /nologo @projectArguments "-getProperty:$propertyNames"
        if ($LASTEXITCODE -ne 0) { throw 'MSBuild could not evaluate the resolved toolchain properties.' }
        $properties = ($propertiesJson -join "`n" | ConvertFrom-Json).Properties
        foreach ($name in $propertyNames.Split(',')) {
            if ([string]::IsNullOrWhiteSpace([string]$properties.$name)) { throw "MSBuild resolved no $name for $Platform/$Configuration." }
        }
        if ($properties.PreferredToolArchitecture -notin @('x86','x64','arm64')) { throw 'MSBuild returned an unknown compiler host architecture.' }
        $hostTool = "Host$($properties.PreferredToolArchitecture)"
        $compiler = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/$Platform/cl.exe"
        $linker = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/$Platform/link.exe"
        $toolsetProps = Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'
        foreach ($path in @($compiler,$linker,$toolsetProps)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Resolved toolchain file is missing: $path" } }
        $sdk = $properties.WindowsTargetPlatformVersion.TrimEnd('\','/')
        $sdkRoot = $properties.WindowsSdkDir.TrimEnd('\','/')
        $sdkDirectories = @((Join-Path $sdkRoot "Include/$sdk/um"),(Join-Path $sdkRoot "Include/$sdk/shared"),(Join-Path $sdkRoot "Include/$sdk/ucrt"),
            (Join-Path $sdkRoot "Lib/$sdk/um/$Platform"),(Join-Path $sdkRoot "Lib/$sdk/ucrt/$Platform"))
        $toolchainFiles = @($msbuildPath,$compiler,$linker,$toolsetProps)
        $toolchainLines = @($properties | Get-Member -MemberType NoteProperty | Sort-Object Name | ForEach-Object { "$($_.Name)=$(Normalize-PerformanceBuildPath ([string]$properties.($_.Name)) $rootPath)" })
        $expandedProject = Get-PreprocessedProjectFileIdentity -MSBuildPath $msbuildPath -ProjectPath $projectPath -Configuration $Configuration -Platform $Platform -OutputRoot $buildOutputRoot -Root $rootPath
        $toolchainLines += "ExpandedProject=$expandedProject"
        $resolvedItemsJson = & $msbuildPath $projectPath /nologo @projectArguments -getItem:ClCompile
        if ($LASTEXITCODE -ne 0) { throw 'MSBuild could not evaluate resolved compiler item metadata.' }
        $resolvedItems = ($resolvedItemsJson -join "`n" | ConvertFrom-Json).Items.ClCompile
        $toolchainLines += "ClCompileItems=$(Get-ResolvedCompileItemsIdentity -Items $resolvedItems -Root $rootPath)"
        $toolchainLines += foreach ($path in $toolchainFiles) { "$([IO.Path]::GetFileName($path)) $((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)" }
        # cl.exe loads backend DLLs, and its installed standard-library headers/libraries affect generated code.
        # A stable executable/version alone cannot attest those mutable toolset inputs.
        $msvcDirectories = @((Join-Path $properties.VCToolsInstallDir 'include'),
            (Join-Path $properties.VCToolsInstallDir "lib/$Platform"), (Split-Path -Parent $compiler))
        $toolchainLines += "MSVCFiles=$(& $hashTree -Paths $msvcDirectories)"
        $toolchainLines += "SDK=$sdk;SDKFiles=$(& $hashTree -Paths $sdkDirectories)"
        $toolchain = & $hashText ($toolchainLines -join "`n")

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
        $environment = & $hashText (@("OS=$([Runtime.InteropServices.RuntimeInformation]::OSDescription)","Architecture=$([Runtime.InteropServices.RuntimeInformation]::OSArchitecture)","Machine=$env:COMPUTERNAME","CPU=$cpu","GPU=$($gpu -join ';')","SystemDpi=$dpi","WARP=$warpSha") -join "`n")
        $status = if ($IdentityScope -eq 'Full') { 'verifiable' } else { 'platform-only' }
        return [ordered]@{ identityStatus=$status; identityScope=$scopeName; harnessIdentity=$harness; toolchainIdentity=$toolchain; dependencyIdentity=$dependency; environmentIdentity=$environment }
    }
    catch {
        $scopeName = if ($IdentityScope -eq 'Full') { 'full' } else { 'platform-only' }
        return [ordered]@{ identityStatus='identity-unverifiable'; identityScope=$scopeName; identityError=$_.Exception.Message; harnessIdentity=''; toolchainIdentity=''; dependencyIdentity=''; environmentIdentity='' }
    }
}

function Test-PerformanceReceiptIdentity {
    [CmdletBinding()]
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Receipt)
    $required = @('sourceCommit','sourceFingerprint','executableSha256','benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity',
        'fixture','renderer','width','height','dpi','controls','modelRows','framesPerRound','roundCount','platform','configuration','nativeArchitecture',
        'machine','cpu','os','compiler','powerPolicy','warpVersion','warpSha256')
    $missing = @($required | Where-Object { -not $Receipt.Contains($_) -or $null -eq $Receipt[$_] -or [string]::IsNullOrWhiteSpace([string]$Receipt[$_]) })
    if (-not $Receipt.Contains('identityStatus') -or $Receipt['identityStatus'] -cne 'verifiable') { $missing += 'identityStatus' }
    if ($Receipt.Contains('identityScope') -and $Receipt['identityScope'] -cne 'full') { $missing += 'identityScope' }
    if ($Receipt.Contains('identityError') -and -not [string]::IsNullOrWhiteSpace([string]$Receipt['identityError'])) { $missing += "identityError:$($Receipt['identityError'])" }
    return [ordered]@{ verifiable=($missing.Count -eq 0); missing=$missing }
}

Export-ModuleMember -Function Get-TrustedPerformancePolicy, Test-PerformanceReceiptIdentity, Get-PerformanceToolIdentities, Get-ResolvedCompileItemsIdentity, Get-PreprocessedProjectIdentity
