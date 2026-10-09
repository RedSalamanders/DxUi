# Repository-owned scope planning. No consumer checkout, service or desktop dependency.
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

function Get-ScopedTrackedPaths {
    param([string] $Root)
    return @((Invoke-ScopedGit $Root @('ls-files','--cached','--others','--exclude-standard','-z')) -split '\x00' | Where-Object { $_ } | Sort-Object -Unique)
}

function Get-ScopedChangedPaths {
    param([string] $Root, [string] $BaseRef)
    $base = (Invoke-ScopedGit $Root @('merge-base',$BaseRef,'HEAD')).Trim()
    $committed = Invoke-ScopedGit $Root @('diff','--name-only','--no-renames','-z',$base,'HEAD','--')
    $staged = Invoke-ScopedGit $Root @('diff','--cached','--name-only','--no-renames','-z','--')
    $working = Invoke-ScopedGit $Root @('diff','--name-only','--no-renames','-z','--')
    $untracked = Invoke-ScopedGit $Root @('ls-files','--others','--exclude-standard','-z')
    return @(($committed + $staged + $working + $untracked) -split '\x00' | Where-Object { $_ } | Sort-Object -Unique)
}

function Test-ScopedPattern {
    param([string] $Path, [string] $Pattern)
    $expression = [regex]::Escape($Pattern).Replace('\*\*/','(?:.*/)?').Replace('\*\*','.*').Replace('\*','[^/]*').Replace('\?','[^/]')
    return [regex]::IsMatch($Path, '^' + $expression + '$', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
}

function Read-ScopedTestManifest {
    param([string] $Root)
    $manifest = Get-Content -LiteralPath (Join-Path $Root 'Tests/test-scopes.json') -Raw | ConvertFrom-Json
    if ($manifest.version -ne 1 -or @($manifest.scopes).Count -eq 0) { throw 'Invalid test scope manifest.' }
    $names = @($manifest.scopes | ForEach-Object { $_.name })
    if (@($names | Sort-Object -Unique).Count -ne $names.Count -or @($names | Where-Object { $_ -notmatch '^[A-Za-z][A-Za-z0-9.-]*$' }).Count) { throw 'Invalid or duplicate test scopes.' }
    foreach ($rule in $manifest.rules) {
        foreach ($name in $rule.scopes) { if ($name -ne '*' -and $name -notin $names) { throw "Rule selects unknown scope '$name'." } }
    }
    return $manifest
}

function Assert-ScopedTestNames {
    param([string] $Root)
    $declared = @(Get-Content -LiteralPath (Join-Path $Root 'Tests/native-test-files.json') -Raw | ConvertFrom-Json)
    if (@($declared | Sort-Object -Unique).Count -ne $declared.Count) { throw 'Duplicate native test inventory paths.' }
    foreach ($path in $declared) {
        if ($path -match '^(Specs|Measurements|legacy|External)/') { throw "Historical/external source cannot enter the active test inventory: $path" }
        if ($path -cnotmatch '(^|/)[^/]+\.Tests\.[^/]+\.(cpp|h)$' -or -not (Test-Path -LiteralPath (Join-Path $Root $path) -PathType Leaf)) { throw "Invalid/missing native test source: $path" }
    }
    $live = @(Get-ScopedTrackedPaths $Root | Where-Object {
        $_ -notmatch '^(Specs|Measurements|legacy|External)/' -and
        $_ -cne 'Tools/TerminalEngine/TerminalEngineGate0ContractTestAdapter.cpp' -and
        $_ -match '\.(cpp|h)$' -and ($_ -match '^Tests/|/SelfTest/|(^|/)[^/]*(Test|Mock|Fake)[^/]*\.(cpp|h)$') -and
        (Test-Path -LiteralPath (Join-Path $Root $_) -PathType Leaf)
    })
    foreach ($path in $live) { if ($path -notin $declared) { throw "Native test source is absent from Tests/native-test-files.json: $path" } }
    return $declared.Count
}

function Get-ScopedTestPlan {
    param([object] $Manifest, [AllowEmptyCollection()][string[]] $ChangedPaths, [string[]] $Scopes = @(), [switch] $Full)
    $names = @($Manifest.scopes | ForEach-Object { $_.name })
    $selected = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $reasons = [Collections.Generic.List[object]]::new()
    foreach ($scope in $Scopes) {
        if ($scope -notin $names) { throw "Unknown test scope '$scope'. Available: $($names -join ', ')" }
        [void]$selected.Add($scope)
        $reasons.Add([pscustomobject]@{path='(explicit)'; scopes=@($scope); reason='explicit scope'})
    }
    if ($Full) { foreach ($name in $names) { [void]$selected.Add($name) } }
    if (-not $Scopes.Count -and -not $Full) {
        foreach ($path in $ChangedPaths) {
            if ($path -match '(^/|(^|/)\.\.(/|$)|:|\\)') { throw "Expected a repository-relative Git path: $path" }
            if ($path -match '\.md$') {
                $targets = @($Manifest.scopes | Where-Object { -not $_.native } | ForEach-Object name)
                foreach ($name in $targets) { [void]$selected.Add($name) }
                $reasons.Add([pscustomobject]@{path=$path; scopes=$targets; reason='documentation/skill validation; no native test input'})
                continue
            }
            $matched = @($Manifest.rules | Where-Object { Test-ScopedPattern $path $_.pattern })
            if ($matched.Count) {
                foreach ($rule in $matched) {
                    $targets = if ('*' -in $rule.scopes) { $names } else { @($rule.scopes) }
                    foreach ($name in $targets) { [void]$selected.Add($name) }
                    $reasons.Add([pscustomobject]@{path=$path; scopes=$targets; reason=$rule.reason})
                }
            } elseif ($path -match '^(docs|Measurements|Changes)/') {
                $targets = @($Manifest.scopes | Where-Object { -not $_.native } | ForEach-Object name)
                foreach ($name in $targets) { [void]$selected.Add($name) }
                $reasons.Add([pscustomobject]@{path=$path; scopes=$targets; reason='non-Markdown documentation/measurement validation; no native test input'})
            } else {
                foreach ($name in $names) { [void]$selected.Add($name) }
                $reasons.Add([pscustomobject]@{path=$path; scopes=$names; reason='unmapped input; conservative full fallback'})
            }
        }
    }
    return [pscustomobject]@{scopes=@($names | Where-Object { $selected.Contains($_) }); reasons=@($reasons); full=($selected.Count -eq $names.Count)}
}

function Get-ScopedInteractiveObligations {
    param([object] $Manifest, [AllowEmptyCollection()][string[]] $ChangedPaths, [switch] $Full)
    $obligations = [Collections.Generic.List[object]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $rules = if ($Manifest.PSObject.Properties['interactiveObligations']) { @($Manifest.interactiveObligations) } else { @() }
    foreach ($rule in $rules) {
        $hasMatch = $Full -or @($ChangedPaths | Where-Object { Test-ScopedPattern $_ $rule.pattern }).Count -gt 0
        if (-not $hasMatch) { continue }
        foreach ($suite in $rule.suites) {
            if (-not $seen.Add([string]$suite)) { continue }
            $obligations.Add([pscustomobject]@{ suite=[string]$suite; pattern=[string]$rule.pattern; reason=[string]$rule.reason })
        }
    }
    return $obligations.ToArray()
}

function Get-ScopedDigest {
    param([string] $Text)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Get-ScopedSourceIdentity {
    param([string] $Root, [switch] $CompiledOnly)
    $rows = [Collections.Generic.List[string]]::new()
    foreach ($path in @(Get-ScopedTrackedPaths $Root)) {
        # Validators read prose, skills and archived mappings too. Their full identity includes every versioned input.
        if ($path -match '^\.build/') { continue }
        # Build attestation excludes immutable evidence/prose, but has no extension whitelist: .inl and new generators count.
        if ($CompiledOnly -and ($path -match '^(Measurements|docs|Changes|legacy|Specs/(Plans|Done|TestRuns|Reviews|Mockups))/' -or $path -match '\.md$')) { continue }
        $full = Join-Path $Root $path
        # A tracked path removed from the worktree is absent source content whether or not
        # the deletion has been staged. The changed-path identity records the deletion.
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            continue
        }
        $value = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
        $rows.Add("$path`0$value")
    }
    return Get-ScopedDigest ($rows -join "`n")
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
    param([string] $Root, [ValidateSet('x64','ARM64')][string] $Platform)
    return Get-ScopedDigest (@((Get-ScopedSourceIdentity -Root $Root -CompiledOnly),
        (Get-ScopedDependencyIdentity -Root $Root -Platform $Platform)) -join "`n")
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

function Get-ScopedToolIdentity {
    # Tooling fixtures invoke Git directly and PR discovery may invoke gh. Bind reuse to their actual binaries.
    $values = [Collections.Generic.List[string]]::new()
    foreach ($name in @('git','gh')) {
        $command = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $command) { $values.Add("${name}:missing"); continue }
        $values.Add("${name}:" + (Get-FileHash -LiteralPath $command.Source -Algorithm SHA256).Hash)
        $version = & $command.Source --version 2>&1
        if ($LASTEXITCODE) { throw "Cannot attest the $name version." }
        $values.Add(($version -join "`n"))
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
    param([string] $Path, [string] $Identity)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    try {
        $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        # PowerShell 7.5 parses ISO JSON strings into DateTime; preserve the canonical UTC spelling on earlier hosts too.
        $completed = if ($receipt.completedUtc -is [DateTime]) { $receipt.completedUtc.ToUniversalTime().ToString('o') } else { [string]$receipt.completedUtc }
        return $receipt.version -eq 1 -and $receipt.outcome -eq 'PASSED' -and $receipt.identity -ceq $Identity -and
            $receipt.digest -ceq (Get-ScopedDigest ($receipt.identity + "`n" + $completed + "`nPASSED"))
    } catch [ArgumentException] { return $false }
    catch [System.Management.Automation.RuntimeException] { return $false }
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

function Get-ScopedPrCandidateScopes {
    param([string] $Root, [object] $Manifest, [string] $Platform, [string] $Configuration,
        [AllowEmptyCollection()][string[]] $ChangedPaths)
    $profile = @($Manifest.prCoverage | Where-Object { $_.platform -eq $Platform -and $_.configuration -eq $Configuration })
    $covered = @($profile | ForEach-Object { $_.scopes } | Sort-Object -Unique)
    if ($Manifest.PSObject.Properties['prNativeScopeModule']) {
        Import-Module (Join-Path $Root $Manifest.prNativeScopeModule) -Force
        if (-not (Get-NativeScope -ChangedPaths $ChangedPaths).Native) {
            $covered = @($Manifest.scopes | Where-Object { -not $_.native -and $_.name -in $covered } | ForEach-Object name)
        }
    }
    return $covered
}

function Get-ScopedPrCoverage {
    param([string] $Root, [object] $Manifest, [string] $Platform, [string] $Configuration)
    $profile = @($Manifest.prCoverage | Where-Object { $_.platform -eq $Platform -and $_.configuration -eq $Configuration })
    if (-not $profile.Count) { return @() }
    # A real open PR executes this exact candidate workflow. Resolve its actual base so stacked PRs are counted correctly.
    try {
        # Detached/default-branch, foreign-remote, dirty and non-PR candidates retain all local obligations.
        $candidate = (Invoke-ScopedGit $Root @('rev-parse','HEAD')).Trim()
        $branch = (Invoke-ScopedGit $Root @('symbolic-ref','--quiet','--short','HEAD')).Trim()
        if (-not $branch -or $branch -ceq $Manifest.defaultBranch) { return @() }
        if (Invoke-ScopedGit $Root @('status','--porcelain','--untracked-files=normal')) { return @() }
        $remote = (Invoke-ScopedGit $Root @('remote','get-url','origin')).Trim() -replace '\.git$',''
        $expectedRemote = "https://github.com/$($Manifest.repository)"
        if ($remote -ine $expectedRemote -and $remote -ine "git@github.com:$($Manifest.repository)" -and
            $remote -ine "ssh://git@github.com/$($Manifest.repository)") { return @() }
        $local = [IO.File]::ReadAllText((Join-Path $Root '.github/workflows/ci.yml')) -replace "`r`n","`n"
        if ((Get-ScopedDigest $local) -cne $Manifest.prWorkflowDigest -or $local -notmatch '(?m)^  pull_request:') { return @() }
        $pr = & gh pr view --json baseRefName,baseRefOid,headRefName,headRefOid,state 2>$null | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or $pr.state -cne 'OPEN' -or $pr.headRefName -cne $branch -or $pr.headRefOid -cne $candidate -or
            [string]::IsNullOrWhiteSpace([string]$pr.baseRefName)) { return @() }
        $workflow = & gh api "repos/$($Manifest.repository)/actions/workflows/ci.yml" 2>$null | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or $workflow.state -ne 'active') { return @() }
        $baseline = (Invoke-ScopedGit $Root @('rev-parse','--verify',"refs/remotes/origin/$($pr.baseRefName)^{commit}")).Trim()
        if (-not $baseline -or $baseline -cne $pr.baseRefOid) { return @() }
        $paths = @(Get-ScopedChangedPaths $Root $baseline)
        $covered = @(Get-ScopedPrCandidateScopes $Root $Manifest $Platform $Configuration $paths)
        if ((Invoke-ScopedGit $Root @('rev-parse','HEAD')).Trim() -cne $candidate -or
            (Invoke-ScopedGit $Root @('symbolic-ref','--quiet','--short','HEAD')).Trim() -cne $branch -or
            (Invoke-ScopedGit $Root @('rev-parse','--verify',"refs/remotes/origin/$($pr.baseRefName)^{commit}")).Trim() -cne $baseline -or
            (Invoke-ScopedGit $Root @('status','--porcelain','--untracked-files=normal'))) { return @() }
        return $covered
    } catch [System.Management.Automation.RuntimeException] { return @() }
    catch [ArgumentException] { return @() }
}

Export-ModuleMember -Function Get-ScopedChangedPaths, Read-ScopedTestManifest, Assert-ScopedTestNames, Get-ScopedTestPlan,
    Get-ScopedSourceIdentity, Get-ScopedDependencyIdentity, Get-ScopedBuildInputIdentity, Get-ScopedArtifactIdentity, Get-ScopedRunIdentity, Get-ScopedDigest,
    Test-ScopedReceipt, Write-ScopedReceipt, Get-ScopedPrCoverage, Get-ScopedPrCandidateScopes, Get-ScopedEnvironmentIdentity,
    Get-ScopedToolIdentity, Get-ScopedInstrumentation, Test-ScopedPattern, Get-ScopedInteractiveObligations
