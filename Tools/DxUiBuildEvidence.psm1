# DxUi's native build, SDK and benchmark evidence. Shared scope planning and receipts use the pinned engine.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-ScopedGit {
    param([string] $Root, [string[]] $Arguments)
    $start = [Diagnostics.ProcessStartInfo]::new('git')
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    foreach ($argument in @('-c','core.quotepath=false','-C',$Root) + $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $errors = $process.StandardError.ReadToEndAsync()
        $output = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        $errorText = $errors.GetAwaiter().GetResult()
        if ($process.ExitCode) { throw "Git scope discovery failed: $errorText" }
        return $output
    } finally { $process.Dispose() }
}

function Get-ScopedOrdinalUniquePaths {
    param([AllowEmptyCollection()][string[]] $Paths)
    # Git path identity is ordinal and case-sensitive, even when the host filesystem is not.
    # SortedSet also provides deterministic ordering without PowerShell's culture/case-insensitive -Unique behavior.
    $unique = [Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
    foreach ($path in $Paths) {
        if (-not [string]::IsNullOrEmpty($path)) { [void]$unique.Add($path) }
    }
    return @($unique)
}

function Get-ScopedTrackedPaths {
    param([string] $Root)
    $paths = (Invoke-ScopedGit $Root @('ls-files','--cached','--others','--exclude-standard','-z')) -split '\x00'
    return @(Get-ScopedOrdinalUniquePaths $paths)
}

function Test-ScopedDocumentationPath {
    param([string] $Path)
    # Markdown fixtures are executable data. Unknown .md locations keep the normal rule/fallback path.
    if ($Path -match '^Tests/(Data|Fixtures)/') { return $false }
    return $Path -match '\.md$' -and ($Path -match '^(Specs|docs|Measurements|Changes|legacy|\.agents|\.github/(skills|ISSUE_TEMPLATE|PULL_REQUEST_TEMPLATE))/' -or
        $Path -match '^[^/]+\.md$|(^|/)(README|AGENTS|CLAUDE)\.md$')
}

function Get-ScopedDigest {
    param([string] $Text)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Get-ScopedSourceIdentity {
    param([string] $Root, [switch] $CompiledOnly, [string] $ToolingEngineRoot = '')
    $source = Get-ScopedMapIdentity (Get-ScopedSourceMap -Root $Root -CompiledOnly:$CompiledOnly)
    if ($ToolingEngineRoot -and -not (Test-Path -LiteralPath (Join-Path $Root '.ci/project.json') -PathType Leaf)) {
        throw 'An explicit tooling engine reader requires the side own .ci/project.json.'
    }
    # Generated engine code is outside Git's product inventory, but executes the governed tools.
    # Keep it separate from iteration maps so it cannot masquerade as a changed product file.
    if (Test-Path -LiteralPath (Join-Path $Root '.ci/project.json') -PathType Leaf) {
        $actualEngine = Join-Path $Root '.ci-engine'
        $readerEngine = if ($ToolingEngineRoot) { [IO.Path]::GetFullPath($ToolingEngineRoot).TrimEnd('\','/') } else { $actualEngine }
        if (-not $ToolingEngineRoot -and -not (Test-Path -LiteralPath (Join-Path $readerEngine 'Tools/Modules/Ci.SourceInputs.psm1') -PathType Leaf)) {
            # Historical SDK sources retain their exact engine, which predates this reader.
            # Use the tooling owner's reader and bind BOTH engines through the distinct key below.
            $readerEngine = Join-Path (Split-Path $PSScriptRoot -Parent) '.ci-engine'
        }
        Import-Module (Join-Path (Split-Path $readerEngine -Parent) '.ci-engine/Tools/Modules/Ci.SourceInputs.psm1') -ErrorAction Stop
        $engineInputs = Get-CiPinnedEngineInputsIdentity -Root $Root -EngineRoot $actualEngine
        $pathComparison=if($IsWindows){[StringComparison]::OrdinalIgnoreCase}else{[StringComparison]::Ordinal}
        if (-not [IO.Path]::GetFullPath($readerEngine).TrimEnd('\','/').Equals([IO.Path]::GetFullPath($actualEngine).TrimEnd('\','/'), $pathComparison)) {
            # Paired diagnostics keep historical source pins while using one frozen current reader.
            # Bind BOTH real independent checkouts; default qualification cannot reuse this key.
            $readerInputs = Get-CiPinnedEngineInputsIdentity -Root (Split-Path $readerEngine -Parent) -EngineRoot $readerEngine
            return Get-ScopedDigest ("$source`0$engineInputs`0$readerInputs")
        }
        return Get-ScopedDigest ("$source`0$engineInputs")
    }
    return $source
}

function Get-ScopedSourceMap {
    param([string] $Root, [switch] $CompiledOnly)
    $map = [Collections.Specialized.OrderedDictionary]::new([StringComparer]::Ordinal)
    foreach ($path in @(Get-ScopedTrackedPaths $Root)) {
        # Validators read prose, skills and archived mappings too. Their full identity includes every versioned input.
        if ($path -match '^\.build/') { continue }
        # Build attestation excludes immutable evidence/prose, but has no extension whitelist: .inl and new generators count.
        if ($CompiledOnly -and ($path -match '^(Measurements|docs|Changes|legacy|Specs/(Plans|SourceImport|TestRuns|Reviews|Mockups))/' -or (Test-ScopedDocumentationPath $path))) { continue }
        $full = Join-Path $Root $path
        # A tracked path removed from the worktree is absent source content whether or not
        # the deletion has been staged. The changed-path identity records the deletion.
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            continue
        }
        $map.Add($path, (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash)
    }
    return ,$map
}

function Get-ScopedMapIdentity {
    param([Collections.IDictionary] $Map)
    $rows = foreach ($path in @(Get-ScopedOrdinalUniquePaths @($Map.Keys))) { "$path`0$($Map[$path])" }
    return Get-ScopedDigest ($rows -join "`n")
}

function Enter-ScopedProfileLease {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string] $OutputRoot = '')
    # Reentrant for a nested test.ps1 call on the owning thread; disjoint profiles/worktrees remain independent.
    if (-not $OutputRoot) { $OutputRoot=Join-Path $Root '.build' }
    $rootIdentity=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
    if ([OperatingSystem]::IsWindows()) { $rootIdentity=$rootIdentity.ToUpperInvariant() }
    $key=Get-ScopedDigest (@($rootIdentity,$Platform,$Configuration) -join "`n")
    $mutex=[Threading.Mutex]::new($false,"DxUi.ScopedProfile.$key")
    $owned=$false
    try {
        try { $owned=$mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned=$true }
        if (-not $owned) { throw 'Another test operation owns this checkout/profile. Retry when it completes.' }
        return $mutex
    } catch { $mutex.Dispose(); throw }
}

function Exit-ScopedProfileLease {
    param([Threading.Mutex] $Lease)
    try { $Lease.ReleaseMutex() } finally { $Lease.Dispose() }
}

function Assert-ScopedIterationPath {
    param([string] $Path, [string] $Root)
    $prefix = [IO.Path]::GetFullPath((Join-Path $Root '.build/reports/scoped-tests')).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Iteration baseline escaped its local report root.' }
    $cursor = $resolved
    while ($cursor.Length -gt [IO.Path]::GetFullPath($Root).TrimEnd('\','/').Length) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse iteration baseline paths are unsupported.' }
        $cursor = Split-Path -Parent $cursor
    }
}

function Remove-ScopedIteration {
    param([string] $Path, [string] $Root)
    Assert-ScopedIterationPath $Path $Root
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -ErrorAction Stop }
}

function Get-ScopedDependencyIdentity {
    param([string] $Root, [ValidateSet('x64','ARM64')][string] $Platform)
    $triplet = if ($Platform -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
    $installedRoot = Join-Path $Root ".build/vcpkg_installed/$Platform"
    $tripletRoot = Join-Path $installedRoot $triplet
    $requiredHeader = Join-Path $tripletRoot 'include/wil/resource.h'
    $statusPath = Join-Path $installedRoot 'vcpkg/status'
    $infoRoot = Join-Path $installedRoot 'vcpkg/info'
    $shareRoot = Join-Path $tripletRoot 'share'
    if (-not (Test-Path -LiteralPath $requiredHeader -PathType Leaf)) { throw "Installed $Platform vcpkg triplet is missing its required WIL header: $requiredHeader" }
    if (-not (Test-Path -LiteralPath $statusPath -PathType Leaf)) { throw "Installed $Platform vcpkg status metadata is missing: $statusPath" }
    if (-not (Test-Path -LiteralPath $infoRoot -PathType Container)) { throw "Installed $Platform vcpkg package inventory is missing: $infoRoot" }
    $packageInfo = @(Get-ChildItem -LiteralPath $infoRoot -File -Filter '*.list' -ErrorAction Stop | Sort-Object Name)
    if (-not $packageInfo.Count) { throw "Installed $Platform vcpkg package inventory has no .list files: $infoRoot" }
    if (-not (Test-Path -LiteralPath $shareRoot -PathType Container)) { throw "Installed $Platform vcpkg ABI metadata directory is missing: $shareRoot" }
    $abiFiles = @(Get-ChildItem -LiteralPath $shareRoot -File -Recurse -ErrorAction Stop | Where-Object Name -in @('vcpkg_abi_info.txt','vcpkg.spdx.json'))
    if (-not $abiFiles.Count) { throw "Installed $Platform vcpkg triplet has no ABI metadata under: $shareRoot" }

    $files = [Collections.Generic.List[IO.FileInfo]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $tripletRoot -File -Recurse -ErrorAction Stop)) { $files.Add($file) }
    $files.Add((Get-Item -LiteralPath $statusPath))
    foreach ($file in $packageInfo) { $files.Add($file) }
    $rows = [Collections.Generic.List[string]]::new()
    $rootPrefix = [IO.Path]::GetFullPath($installedRoot).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\','/')
        $rows.Add($relative + ':' + (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())
    }
    $orderedRows = $rows.ToArray()
    [Array]::Sort($orderedRows,[StringComparer]::Ordinal)
    return Get-ScopedDigest ($orderedRows -join "`n")
}

function Get-ScopedBuildInputIdentity {
    param([string] $Root, [ValidateSet('x64','ARM64')][string] $Platform, [string] $ToolingEngineRoot = '')
    $readerArguments=@{};if($ToolingEngineRoot){$readerArguments.ToolingEngineRoot=$ToolingEngineRoot}
    return Get-ScopedDigest (@((Get-ScopedSourceIdentity -Root $Root -CompiledOnly @readerArguments),
        (Get-ScopedDependencyIdentity -Root $Root -Platform $Platform)) -join "`n")
}

function Get-ScopedSuiteProjectAliases {
    return [ordered]@{
        'Tests/FoundationTests/FoundationTests.vcxproj' = 'Tests/Foundation/DxUi.FoundationTests.vcxproj'
        'Tests/ControlTests/ControlTests.vcxproj' = 'Tests/Controls/DxUi.ControlTests.vcxproj'
        'Tests/EmbeddedTests/EmbeddedTests.vcxproj' = 'Tests/Embedded/DxUi.EmbeddedTests.vcxproj'
    }
}

function Get-ScopedBuildProjectCatalog {
    param([string] $Root)
    $catalog = [ordered]@{
        'DxUi'='src/DxUi.vcxproj'
        'DxUi.FoundationTests'='Tests/FoundationTests/FoundationTests.vcxproj'
        'DxUi.ControlTests'='Tests/ControlTests/ControlTests.vcxproj'
        'DxUi.EmbeddedControls'='Samples/EmbeddedControls/DxUi.EmbeddedControls.vcxproj'
        'DxUi.EmbeddedTests'='Tests/EmbeddedTests/EmbeddedTests.vcxproj'
        'DxUi.InteractiveLease'='Tests/InteractiveLease/DxUi.InteractiveLease.vcxproj'
    }
    if ($Root) {
        $solutionPath = Join-Path $Root 'DxUi.sln'
        if (Test-Path -LiteralPath $solutionPath -PathType Leaf) {
            $solution = [IO.File]::ReadAllText($solutionPath)
            $aliases = Get-ScopedSuiteProjectAliases
            foreach ($name in @($catalog.Keys)) {
                $legacy = $aliases[$catalog[$name]]
                if ($legacy -and $solution.Contains('"' + $legacy.Replace('/','\') + '"', [StringComparison]::OrdinalIgnoreCase)) {
                    $catalog[$name] = $legacy
                }
            }
        }
    }
    return $catalog
}

function Resolve-ScopedBuildProjects {
    param([string[]] $Projects)
    $catalog=Get-ScopedBuildProjectCatalog
    foreach ($project in $Projects) { if ($project -cnotin @($catalog.Keys)) { throw "Unknown native build project: $project" } }
    # All current application/test projects reference the single static library.
    return @($catalog.Keys | Where-Object { $_ -ceq 'DxUi' -or $_ -cin $Projects })
}

function Get-ScopedRequiredBuildProjects {
    param([string[]] $Suites, [string] $Configuration, [switch] $Full)
    if ($Full) { return @((Get-ScopedBuildProjectCatalog).Keys) }
    # Every test invocation runs performance.ps1 through the benchmark executable.
    $projects=@('DxUi','DxUi.EmbeddedTests')
    if ('Foundation' -in $Suites -or $Configuration -eq 'ASan Debug') { $projects+='DxUi.FoundationTests' }
    if (@($Suites | Where-Object { $_ -notin @('Foundation','Embedded') }).Count) { $projects+='DxUi.ControlTests' }
    if (@($Suites | Where-Object { $_ -in @('InteractiveLease','Menu','NativeTextInput','MenuResources','MenuResourceScaling','MenuTextLayoutResources') }).Count) { $projects+='DxUi.InteractiveLease' }
    return @(Resolve-ScopedBuildProjects $projects)
}

function Assert-ScopedSelectedBuildGraph {
    param([string] $Root, [string] $MSBuildPath, [string] $Platform, [string] $Configuration, [string] $OutputRoot, [string[]] $Projects)
    $catalog=Get-ScopedBuildProjectCatalog -Root $Root
    $selected=@(Resolve-ScopedBuildProjects $Projects)
    # Evaluate actual references, including imported and conditional items. Opaque/new
    # dependencies refuse partial admission until their build/runtime contract is reviewed.
    foreach ($project in $selected) {
        $projectPath=[IO.Path]::GetFullPath((Join-Path $Root $catalog[$project]))
        $text=& $MSBuildPath $projectPath /nologo "/p:Platform=$Platform" "/p:Configuration=$Configuration" "/p:DxUiOutputRoot=$OutputRoot" '-getItem:ProjectReference' '-getProperty:TargetPath,ConfigurationType'
        if ($LASTEXITCODE) { throw "Cannot evaluate selected native project: $project" }
        $evaluation=($text -join "`n") | ConvertFrom-Json
        $extension=if ($project -ceq 'DxUi') {'.lib'} else {'.exe'}
        $expected=[IO.Path]::GetFullPath((Join-Path $OutputRoot "$Platform/$Configuration/$project$extension"))
        if ([IO.Path]::GetFullPath($evaluation.Properties.TargetPath) -ine $expected -or
            $evaluation.Properties.ConfigurationType -cne $(if ($project -ceq 'DxUi') {'StaticLibrary'} else {'Application'})) { throw "Unsupported selected output contract: $project" }
        $references=@($evaluation.Items.ProjectReference)
        if (($project -ceq 'DxUi' -and $references.Count) -or ($project -cne 'DxUi' -and $references.Count -ne 1)) { throw "Unsupported selected reference inventory: $project" }
        foreach ($reference in $references) {
            $path=[IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $projectPath) $reference.Identity))
            if ($path -ine [IO.Path]::GetFullPath((Join-Path $Root $catalog['DxUi']))) { throw "Unreviewed selected native dependency: $project" }
        }
    }
}

function Assert-ScopedBuildOutputs {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string[]] $Projects)
    $profile=Join-Path $Root ".build/$Platform/$Configuration"
    $cursor=[IO.Path]::GetFullPath($profile)
    while ($cursor.Length -gt [IO.Path]::GetFullPath($Root).TrimEnd('\','/').Length) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse selected build profile is unsupported.' }
        $cursor=Split-Path -Parent $cursor
    }
    $outputs=@(foreach ($project in @(Resolve-ScopedBuildProjects $Projects)) {
        if ($project -ceq 'DxUi') { 'DxUi.lib'; 'DxUi.pdb' } else { "$project.exe"; "$project.pdb" }
    })
    if ($Configuration -eq 'ASan Debug') { $outputs+=if ($Platform -eq 'ARM64') {'clang_rt.asan_dynamic-aarch64.dll'} else {'clang_rt.asan_dynamic-x86_64.dll'} }
    foreach ($name in $outputs) {
        $path=Join-Path $profile $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) { throw "Incomplete selected build output: $name" }
        if ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse selected build output: $name" }
    }
}

function Get-ScopedTargetBuildReceipt {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string[]] $Projects)
    $targets=@(Resolve-ScopedBuildProjects $Projects) -join "`n"
    $path=Join-Path $Root ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')/build-targets/$(Get-ScopedDigest $targets).json"
    Assert-ScopedIterationPath $path $Root
    return $path
}

function Remove-ScopedBuildReceipts {
    param([string] $Root, [string] $Platform, [string] $Configuration)
    $profile=Join-Path $Root ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')"
    $full=Get-DxUiNativeBuildReceiptPath $Root $Platform $Configuration
    Assert-ScopedIterationPath $full $Root
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full }
    $targets=Join-Path $profile 'build-targets'
    Assert-ScopedIterationPath (Join-Path $targets 'probe.json') $Root
    if (Test-Path -LiteralPath $targets -PathType Container) {
        foreach ($file in @(Get-ChildItem -LiteralPath $targets -File -Filter '*.json')) {
            Assert-ScopedIterationPath $file.FullName $Root
            Remove-Item -LiteralPath $file.FullName
        }
    }
}

function Write-ScopedTargetBuildReceipt {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string[]] $Projects, [string] $InputIdentity, [string] $ArtifactIdentity)
    Assert-ScopedBuildOutputs $Root $Platform $Configuration $Projects
    $targets=@(Resolve-ScopedBuildProjects $Projects) -join "`n"
    Write-ScopedReceipt (Get-ScopedTargetBuildReceipt $Root $Platform $Configuration $Projects) (Get-ScopedDigest (@('dxui-selected-build-v1',$targets,$InputIdentity,$ArtifactIdentity) -join "`n"))
}

function Test-ScopedBuildCoverage {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string[]] $Projects, [string] $InputIdentity, [string] $ArtifactIdentity, [switch] $Full)
    Assert-ScopedBuildOutputs $Root $Platform $Configuration $Projects
    $report=Get-DxUiNativeBuildReceiptPath $Root $Platform $Configuration
    if (Test-ScopedReceipt $report (Get-ScopedDigest ($InputIdentity+"`n"+$ArtifactIdentity))) { return $true }
    if ($Full) { return $false }
    # Six bounded catalog entries => at most 64 exact, content-bound candidate sets.
    # A larger selected union can satisfy a subset; a partial receipt never grants Full.
    $catalog=@((Get-ScopedBuildProjectCatalog).Keys)
    for ($mask=1; $mask -lt (1 -shl $catalog.Count); $mask++) {
        $set=@(for ($i=0; $i -lt $catalog.Count; $i++) { if ($mask -band (1 -shl $i)) { $catalog[$i] } })
        if ('DxUi' -cnotin $set -or @($Projects | Where-Object { $_ -cnotin $set }).Count) { continue }
        $targets=$set -join "`n"
        $identity=Get-ScopedDigest (@('dxui-selected-build-v1',$targets,$InputIdentity,$ArtifactIdentity) -join "`n")
        if (Test-ScopedReceipt (Get-ScopedTargetBuildReceipt $Root $Platform $Configuration $set) $identity) { return $true }
    }
    return $false
}

function Get-ScopedArtifactIdentity {
    param([string] $Root, [string] $Platform, [string] $Configuration)
    $directory = Join-Path $Root ".build/$Platform/$Configuration"
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "Missing build profile: $directory" }
    $rows = @(Get-ChildItem -LiteralPath $directory -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll','.lib','.pdb') } | Sort-Object FullName | ForEach-Object {
        [IO.Path]::GetRelativePath($directory,$_.FullName) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    if (-not $rows.Count) { throw 'No executable, library, or debug-symbol build artifacts were found.' }
    return Get-ScopedDigest ($rows -join "`n")
}

function Get-ScopedEnvironmentIdentity {
    # No secrets are stored. Different machines, OS/PowerShell, graphics runtime or sanitizer options cannot reuse evidence.
    $values = @([Environment]::MachineName,[Runtime.InteropServices.RuntimeInformation]::OSDescription,
        [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString(),$PSVersionTable.PSVersion.ToString(),
        $env:ASAN_OPTIONS,$env:CI,$env:GITHUB_ACTIONS,$env:PROCESSOR_IDENTIFIER,$env:PATH,
        $env:VCToolsVersion,$env:WindowsSDKVersion)
    # Opt-in instrumentation changes test execution; hash values without recording local paths.
    foreach ($name in @('DXUI_GRAPH_PERF','DXUI_PERF_JSONL_PATH','DXUI_MUTANT','DXUI_ONLY_FRENCH')) {
        $value = [Environment]::GetEnvironmentVariable($name)
        $valueHash = if ($null -eq $value) { 'unset' } else { Get-ScopedDigest $value }
        $values += "$name=$valueHash"
    }
    foreach ($file in @('d3d10warp.dll','d3d11.dll','dwrite.dll')) {
        if (-not $env:SystemRoot) { continue }
        $path = Join-Path $env:SystemRoot "System32/$file"
        if (Test-Path -LiteralPath $path) { $values += (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
    # Tooling fixtures can discover or invoke the installed compiler. Updates must invalidate their receipts too.
    $vswhere = @(${env:ProgramFiles(x86)},$env:ProgramFiles | Where-Object { $_ } | ForEach-Object {
        Join-Path $_ 'Microsoft Visual Studio/Installer/vswhere.exe'
    }) | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
    if ($vswhere) {
        $installations = & $vswhere -all -prerelease -products '*' -format json
        if ($LASTEXITCODE) { throw 'Cannot attest the installed Visual Studio environment.' }
        $values += ($installations -join "`n")
        foreach ($installation in @($installations -join "`n" | ConvertFrom-Json)) {
            $toolset = Join-Path $installation.installationPath 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'
            if (Test-Path -LiteralPath $toolset) { $values += (Get-FileHash -LiteralPath $toolset).Hash }
        }
    }
    if ($env:SystemRoot -and (Test-Path 'HKLM:/SOFTWARE/Microsoft/Windows Kits/Installed Roots')) {
        $values += (Get-ChildItem 'HKLM:/SOFTWARE/Microsoft/Windows Kits/Installed Roots' | Sort-Object Name | ForEach-Object Name) -join "`n"
    }
    return Get-ScopedDigest ($values -join "`n")
}

function Get-ScopedInstrumentation {
    # Ordinary qualification receipts never cover opt-in measurement or mutation runs.
    $active = [Collections.Generic.List[string]]::new()
    foreach ($name in @('DXUI_GRAPH_PERF','DXUI_PERF_JSONL_PATH','DXUI_MUTANT','DXUI_ONLY_FRENCH')) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if (-not [string]::IsNullOrWhiteSpace($value)) { $active.Add($name) }
    }
    return $active.ToArray()
}

function Get-ScopedRunIdentity {
    param([string] $Root, [string] $Platform, [string] $Configuration, [string] $Scope, [string] $Options = '')
    return Get-ScopedDigest (@([IO.Path]::GetFullPath($Root),$Platform,$Configuration,$Scope,$Options,
        (Get-ScopedBuildInputIdentity $Root $Platform),(Get-ScopedArtifactIdentity $Root $Platform $Configuration),
        (Get-ScopedEnvironmentIdentity)) -join "`n")
}

function Test-ScopedReceipt {
    param([string] $Path, [string] $Identity, [ValidateRange(0,8760)][int] $MaxAgeHours = 0)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    try {
        $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        # PowerShell 7.5 parses ISO JSON strings into DateTime; preserve the canonical UTC spelling on earlier hosts too.
        $completed = if ($receipt.completedUtc -is [DateTime]) { $receipt.completedUtc.ToUniversalTime().ToString('o') } else { [string]$receipt.completedUtc }
        $age = [DateTime]::UtcNow - [DateTime]::Parse($completed, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
        return $receipt.version -eq 1 -and $receipt.outcome -eq 'PASSED' -and $receipt.identity -ceq $Identity -and
            $age.TotalHours -ge 0 -and ($MaxAgeHours -eq 0 -or $age.TotalHours -lt $MaxAgeHours) -and
            $receipt.digest -ceq (Get-ScopedDigest ($receipt.identity + "`n" + $completed + "`nPASSED"))
    } catch [ArgumentException] { return $false }
    catch [System.Management.Automation.RuntimeException] { return $false }
}

function Invoke-ScopedHarnessPreflight {
    # Only deterministic filter/watchdog verification is reusable. Desktop availability,
    # foreground leases, performance and product suites still execute under their owners.
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $Root,
        [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string] $Platform,
        [Parameter(Mandatory)][ValidateSet('Debug','Release','ASan Debug')][string] $Configuration,
        [switch] $Force)
    $hostPath = (Get-Process -Id $PID).Path
    $options = 'filter-watchdog-v1:' + (Get-FileHash -LiteralPath $hostPath -Algorithm SHA256).Hash
    $before = Get-ScopedRunIdentity $Root $Platform $Configuration '(harness-preflight)' $options
    $path = Join-Path $Root ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')/harness.json"
    $instrumented = @(Get-ScopedInstrumentation).Count -gt 0
    $fresh = $Force -or $instrumented -or $env:CI -eq 'true' -or $env:GITHUB_ACTIONS -eq 'true'
    if (-not $fresh -and (Test-ScopedReceipt $path $before -MaxAgeHours 24)) {
        Write-Host 'REUSED native filter/watchdog preflight (exact local inputs, artifacts and environment; under 24 hours)'
        return
    }
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    foreach ($script in @('Test-TestFilter.ps1','Test-TestWatchdog.ps1')) {
        & $hostPath -NoProfile -NonInteractive -File (Join-Path $Root "Tools/tests/$script") -Platform $Platform -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) { throw "Native harness preflight failed: $script ($LASTEXITCODE)." }
    }
    if ((Get-ScopedRunIdentity $Root $Platform $Configuration '(harness-preflight)' $options) -cne $before) {
        throw 'Harness preflight inputs or artifacts changed during verification; no reusable result was published.'
    }
    if (-not $instrumented) { Write-ScopedReceipt $path $before }
}

function Write-ScopedReceipt {
    param([string] $Path, [string] $Identity)
    $completed = [DateTime]::UtcNow.ToString('o')
    $receipt = [ordered]@{version=1; outcome='PASSED'; identity=$Identity; completedUtc=$completed;
        digest=(Get-ScopedDigest ($Identity + "`n" + $completed + "`nPASSED"))}
    [void](New-Item -ItemType Directory -Path (Split-Path $Path) -Force)
    $temporary = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllText($temporary,($receipt | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
}

function Get-DxUiNativeBuildReceiptPath {
    param([string] $Root, [string] $Platform, [string] $Configuration)
    $profile = Join-Path $Root ".build/reports/scoped-tests/$Platform-$($Configuration -replace ' ','')"
    $path = Join-Path $profile 'native-build.json'
    Assert-ScopedIterationPath $path $Root
    return $path
}

Export-ModuleMember -Function Get-ScopedSourceIdentity,
    Get-ScopedDependencyIdentity,
    Get-ScopedBuildInputIdentity,
    Get-ScopedArtifactIdentity,
    Get-ScopedDigest,
    Get-ScopedEnvironmentIdentity,
    Get-ScopedInstrumentation,
    Invoke-ScopedHarnessPreflight,
    Get-ScopedBuildProjectCatalog,
    Resolve-ScopedBuildProjects,
    Get-ScopedSuiteProjectAliases,
    Get-ScopedRequiredBuildProjects,
    Assert-ScopedSelectedBuildGraph,
    Assert-ScopedBuildOutputs,
    Get-ScopedTargetBuildReceipt,
    Write-ScopedTargetBuildReceipt,
    Test-ScopedBuildCoverage,
    Remove-ScopedBuildReceipts,
    Test-ScopedReceipt,
    Write-ScopedReceipt,
    Enter-ScopedProfileLease,
    Exit-ScopedProfileLease,
    Remove-ScopedIteration,
    Get-DxUiNativeBuildReceiptPath
