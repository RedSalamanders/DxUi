# Planning for performance-paired.ps1: which trees are measured, whether they differ, and how this checkout's harness
# reaches them. Decisions are pure and the file operations small, so the tooling tests drive them against fixture trees
# without a build. A side is an ordered dictionary: Role (baseline or candidate), Kind (revision, path or checkout), Spec
# (what the caller typed), and once resolved Root (the tree measured), Commit and Fingerprint.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1')

$script:PathComparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
# The measurement driver and its comparator; the compiled benchmark inputs come from the comparator module, which also
# hashes them into every receipt.
$script:HarnessScripts = @('performance.ps1', 'performance-paired.ps1', 'Tools/Compare-Performance.ps1', 'Tools/PerformanceComparison.psm1',
    'Tools/PairedRun.psm1', 'Tools/PerformancePolicy.psm1', 'Tools/PerformanceAcceptancePolicy.v1.json', 'Tools/BenchmarkGate.psm1')
# Old revisions compile their original include names. Overlay the current payload
# at those existing paths too, so renaming a fixture cannot leave an old fixture in use.
$script:HarnessLegacyInputs = [ordered]@{
    'Tests/Embedded/Embedded.Tests.BenchmarkMain.h' = 'Tests/Embedded/BenchmarkMain.h'
    'Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h' = 'Tests/Embedded/ComplexUiBenchmark.h'
    'Tests/Support/Support.Tests.HeapDiagnostic.h' = 'Tests/Support/HeapDiagnostic.h'
}

function Get-PairedHarness {
    <# The files copied from this checkout onto both trees, so one driver, one comparator and one fixture measure them. #>
    return @($script:HarnessScripts) + @(Get-BenchmarkInputPaths)
}

function Get-PairedSelection {
    <# Which tree is A and which is B. The baseline is a revision or a named tree; the candidate is a revision, a named tree
       or, when neither is given, this checkout. An empty value is not given (the hosted workflow passes empty inputs). #>
    param([string] $BaselineRevision = '', [string] $BaselinePath = '', [string] $CandidateRevision = '', [string] $CandidatePath = '')
    if ($BaselineRevision -and $BaselinePath) { throw 'Name the baseline with -BaselineRevision or -BaselinePath, not both.' }
    if (-not $BaselineRevision -and -not $BaselinePath) { throw 'Name the baseline with -BaselineRevision or -BaselinePath.' }
    if ($CandidateRevision -and $CandidatePath) { throw 'Name the candidate with -CandidateRevision or -CandidatePath, not both.' }
    $baseline = if ($BaselinePath) { [ordered]@{ Role = 'baseline'; Kind = 'path'; Spec = $BaselinePath } }
    else { [ordered]@{ Role = 'baseline'; Kind = 'revision'; Spec = $BaselineRevision } }
    $candidate = if ($CandidatePath) { [ordered]@{ Role = 'candidate'; Kind = 'path'; Spec = $CandidatePath } }
    elseif ($CandidateRevision) { [ordered]@{ Role = 'candidate'; Kind = 'revision'; Spec = $CandidateRevision } }
    else { [ordered]@{ Role = 'candidate'; Kind = 'checkout'; Spec = 'HEAD' } }
    return [ordered]@{ Baseline = $baseline; Candidate = $candidate }
}

function New-PairedExecutionPlan {
    <# Assign physical roots to measurement roles. Calibration deliberately shares one worktree and one build so
       same-source runs attest the same executable bytes even when the linker embeds the absolute PDB path. #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][System.Collections.IDictionary] $Baseline,
        [Parameter(Mandatory)][System.Collections.IDictionary] $Candidate,
        [Parameter(Mandatory)][string] $RunRoot,
        [switch] $CalibrationAA
    )
    if ($CalibrationAA) {
        if ($Baseline['Kind'] -cne 'revision' -or $Candidate['Kind'] -cne 'revision' -or
            [string]::IsNullOrWhiteSpace([string]$Baseline['Commit']) -or [string]$Baseline['Commit'] -cne [string]$Candidate['Commit']) {
            throw '-CalibrationAA requires -BaselineRevision and -CandidateRevision naming the same explicit commit.'
        }
        $sharedRoot = Join-Path ([IO.Path]::GetFullPath($RunRoot)) 'baseline'
        $Baseline['Root'] = $sharedRoot
        $Candidate['Root'] = $sharedRoot
        $physical = @($Baseline)
        $created = @($Baseline)
    } else {
        foreach ($side in @($Baseline,$Candidate)) {
            if ($side['Kind'] -ceq 'revision') { $side['Root'] = Join-Path ([IO.Path]::GetFullPath($RunRoot)) ([string]$side['Role']) }
        }
        $physical = @($Baseline,$Candidate)
        $created = @($physical | Where-Object { $_['Kind'] -ceq 'revision' })
    }
    return [ordered]@{
        Baseline=$Baseline; Candidate=$Candidate; PhysicalSides=$physical; CreatedSides=$created
        RoleRoots=[ordered]@{ baseline=[string]$Baseline['Root']; candidate=[string]$Candidate['Root'] }
    }
}

function Test-SamePath([string] $Left, [string] $Right) {
    [char[]] $trim = '\', '/'
    return [string]::Equals([IO.Path]::GetFullPath($Left).TrimEnd($trim), [IO.Path]::GetFullPath($Right).TrimEnd($trim), $script:PathComparison)
}

function Assert-PairedSidesDiffer {
    <# Refuses a pair with nothing to compare. Revisions, and this checkout, differ by commit. A named tree can hold
       uncommitted work on the very commit the other side names, so a pair with one differs by library source fingerprint
       instead; a fingerprint that is not known yet defers that refusal until it is. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Baseline, [Parameter(Mandatory)][System.Collections.IDictionary] $Candidate)
    if ($Baseline['Kind'] -cne 'path' -and $Candidate['Kind'] -cne 'path') {
        if ([string]::Equals($Baseline['Commit'], $Candidate['Commit'], [StringComparison]::Ordinal)) {
            throw 'The baseline and candidate are the same commit. Name this checkout or another tree with -CandidatePath to measure uncommitted work against a revision.'
        }
        return
    }
    if ($Baseline['Root'] -and $Candidate['Root'] -and (Test-SamePath $Baseline['Root'] $Candidate['Root'])) {
        throw "The baseline and candidate name the same tree: $($Baseline['Root'])"
    }
    if ($Baseline['Fingerprint'] -and $Candidate['Fingerprint'] -and [string]::Equals($Baseline['Fingerprint'], $Candidate['Fingerprint'], [StringComparison]::Ordinal)) {
        throw "The baseline and candidate have identical library sources (fingerprint $($Baseline['Fingerprint'])); there is nothing to compare."
    }
}

function Assert-PairedTree {
    <# A named tree must be the top of a git working tree (performance.ps1 records its commit and library inputs from git,
       and inside another repository it would record that one) with the scripts a paired run calls. Returns its full path. #>
    param([Parameter(Mandatory)][string] $Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.Directory]::Exists($full)) { throw "The named tree does not exist: $full" }
    $top = @(& git -C $full rev-parse --show-toplevel 2>$null)
    if ($LASTEXITCODE -ne 0 -or $top.Count -ne 1 -or -not $top[0]) { throw "The named tree is not a git working tree: $full" }
    if (-not (Test-SamePath $top[0] $full)) { throw "The named tree must be the top of its git working tree ($($top[0])): $full" }
    foreach ($name in @('build.ps1', 'vcpkg-install.ps1', 'src', 'include')) {
        if (-not (Test-Path -LiteralPath (Join-Path $full $name))) { throw "The named tree is not a DxUi tree; it has no ${name}: $full" }
    }
    return $full
}

function Copy-HarnessOverlay {
    <# Copies the harness files from Source onto Target. A file the tree already has byte for byte is left alone, keeping
       its timestamp so no build sees a change; any other is written with the current time, because a copy keeps the
       source's older stamp and an existing build would not notice a replaced header. With a BackupDirectory, a replaced
       file is saved there so Restore-HarnessOverlay can put the tree back. Returns one record per path: its action
       (unchanged, created or replaced) and the directories created for it. #>
    param([Parameter(Mandatory)][string] $Source, [Parameter(Mandatory)][string] $Target, [Parameter(Mandatory)][string[]] $Paths, [string] $BackupDirectory = '')
    $same = Test-SamePath $Source $Target
    $records = [Collections.Generic.List[object]]::new()
    try {
        $inputs = foreach ($path in $Paths) {
            [ordered]@{ source = $path; target = $path }
            if (-not $same -and $script:HarnessLegacyInputs.Contains($path)) {
                $legacy = $script:HarnessLegacyInputs[$path]
                if (Test-Path -LiteralPath (Join-Path $Target $legacy) -PathType Leaf) {
                    [ordered]@{ source = $path; target = $legacy }
                }
            }
        }
        foreach ($inputFile in $inputs) {
            $path = $inputFile.target
            $from = Join-Path $Source $inputFile.source
            $to = Join-Path $Target $path
            if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { throw "Missing benchmark harness input: $path" }
            $action = if ($same) { 'unchanged' }
            elseif (-not (Test-Path -LiteralPath $to -PathType Leaf)) { 'created' }
            elseif ((Get-FileHash -LiteralPath $from -Algorithm SHA256).Hash -ceq (Get-FileHash -LiteralPath $to -Algorithm SHA256).Hash) { 'unchanged' }
            else { 'replaced' }
            $created = [Collections.Generic.List[string]]::new()
            if ($action -ne 'unchanged') {
                if ($action -eq 'replaced' -and $BackupDirectory) {
                    $backup = Join-Path $BackupDirectory $path
                    New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
                    Copy-Item -LiteralPath $to -Destination $backup -Force
                }
                # The missing ancestors, outermost first, so a restore can remove exactly what this file added.
                $missing = [Collections.Generic.Stack[string]]::new()
                for ($directory = Split-Path $to; $directory -and -not (Test-Path -LiteralPath $directory); $directory = Split-Path $directory) { $missing.Push($directory) }
                foreach ($directory in $missing) { $created.Add($directory) }
                New-Item -ItemType Directory -Path (Split-Path $to) -Force | Out-Null
                Copy-Item -LiteralPath $from -Destination $to -Force
                [IO.File]::SetLastWriteTimeUtc($to, [DateTime]::UtcNow)
            }
            $records.Add([ordered]@{ path = $path; action = $action; createdDirectories = @($created) })
        }
    } catch {
        # Leave the tree as it was found, even for a half-applied overlay.
        if ($BackupDirectory) { Restore-HarnessOverlay -Target $Target -Records $records.ToArray() -BackupDirectory $BackupDirectory }
        throw
    }
    return $records.ToArray()
}

function Restore-HarnessOverlay {
    <# Undoes Copy-HarnessOverlay: replaced files return from the backup, created files and the directories made for them
       are removed. Restored files are stamped with the current time so the tree's next build sees them change. #>
    param([Parameter(Mandatory)][string] $Target, [Parameter(Mandatory)][object[]] $Records, [Parameter(Mandatory)][string] $BackupDirectory)
    $directories = [Collections.Generic.List[string]]::new()
    foreach ($record in $Records) {
        $to = Join-Path $Target $record['path']
        if ($record['action'] -ceq 'replaced') {
            Copy-Item -LiteralPath (Join-Path $BackupDirectory $record['path']) -Destination $to -Force
            [IO.File]::SetLastWriteTimeUtc($to, [DateTime]::UtcNow)
        } elseif ($record['action'] -ceq 'created') {
            if (Test-Path -LiteralPath $to -PathType Leaf) { Remove-Item -LiteralPath $to -Force }
            $directories.AddRange([string[]]@($record['createdDirectories']))
        }
    }
    # Deepest first, and only what is empty: a directory that gained other files is not this overlay's to remove.
    foreach ($directory in @($directories | Sort-Object -Unique | Sort-Object -Property Length -Descending)) {
        if ([IO.Directory]::Exists($directory) -and -not [IO.Directory]::GetFileSystemEntries($directory).Length) { Remove-Item -LiteralPath $directory -Force }
    }
}

function Get-PairedRunSchedule {
    <# The order the runs go in: the interleaved pass A, B, B, A repeated Repetitions times, so a machine that drifts
       either way lands on both sides. Runs are numbered per side as they run (A1 B1 B2 A2 A3 B3 B4 A4 ...), which keeps
       the first pass's names, and every pass keeps its own two crossings and two same-binary controls under the names a
       single pass has always used. Steps name the tree each run measures; comparisons name a candidate and a baseline run. #>
    param([ValidateRange(1, 10)][int] $Repetitions = 3)
    $steps = [Collections.Generic.List[object]]::new()
    $comparisons = [Collections.Generic.List[object]]::new()
    for ($pass = 1; $pass -le $Repetitions; $pass++) {
        $first = 2 * $pass - 1
        $second = 2 * $pass
        foreach ($run in @(@('A', $first), @('B', $first), @('B', $second), @('A', $second))) {
            $steps.Add([ordered]@{ Name = "$($run[0])$($run[1])"; Side = $(if ($run[0] -ceq 'A') { 'baseline' } else { 'candidate' }) })
        }
        $comparisons.Add([ordered]@{ Name = "B$first-vs-A$first"; Candidate = "B$first"; Baseline = "A$first"; Control = $false })
        $comparisons.Add([ordered]@{ Name = "B$second-vs-A$second"; Candidate = "B$second"; Baseline = "A$second"; Control = $false })
        $comparisons.Add([ordered]@{ Name = "A$second-vs-A$first-control"; Candidate = "A$second"; Baseline = "A$first"; Control = $true })
        $comparisons.Add([ordered]@{ Name = "B$second-vs-B$first-control"; Candidate = "B$second"; Baseline = "B$first"; Control = $true })
    }
    return [ordered]@{ Steps = $steps.ToArray(); Comparisons = $comparisons.ToArray(); Order = (($steps | ForEach-Object { $_['Name'] }) -join ', ') }
}

function Get-RandomizedPairedBlockSchedule {
    <# Each block independently draws ABBA or BAAB with equal probability. Both orders balance the two runs per side
       within a block; there is no constraint on order counts across blocks. Retain the protocol, seed and literal order. #>
    [CmdletBinding()]
    param([ValidateRange(12, 20)][int] $Blocks = 12, [Parameter(Mandatory)][int] $Seed)
    $random = [Random]::new($Seed)
    $steps = [Collections.Generic.List[object]]::new()
    $blockRecords = [Collections.Generic.List[object]]::new()
    $sideRun = @{ A = 0; B = 0 }
    for ($blockIndex = 0; $blockIndex -lt $Blocks; $blockIndex++) {
        $blockName = 'block-{0:D2}' -f ($blockIndex + 1)
        $order = if ($random.Next(2) -eq 0) { 'ABBA' } else { 'BAAB' }
        $aNames = [Collections.Generic.List[string]]::new(); $bNames = [Collections.Generic.List[string]]::new()
        foreach ($sideLetter in $order.ToCharArray()) {
            $sideRun[[string]$sideLetter]++
            $name = '{0}{1:D2}' -f $sideLetter, $sideRun[[string]$sideLetter]
            if ($sideLetter -eq 'A') { $aNames.Add($name) } else { $bNames.Add($name) }
            $steps.Add([ordered]@{ Name=$name; Side=$(if ($sideLetter -eq 'A') { 'baseline' } else { 'candidate' }); Block=$blockName; Position=$steps.Count + 1; Order=$order })
        }
        $blockRecords.Add([ordered]@{ Name=$blockName; Order=$order; BaselineRuns=$aNames.ToArray(); CandidateRuns=$bNames.ToArray() })
    }
    return [ordered]@{ SchemaVersion=3; Seed=$Seed; BlockCount=$Blocks; Allocation=(Get-PairedAssignmentProtocol); Blocks=$blockRecords.ToArray(); Steps=$steps.ToArray(); Order=(($steps | ForEach-Object { $_.Name }) -join ', ') }
}

function Get-VersionedPerformanceJudge {
    <# Load the measured base's judge source from its immutable Git object into an isolated module scope. Never substitute the
       candidate's currently loaded comparison function for a missing or unreadable base judge. #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $RepositoryRoot, [Parameter(Mandatory)][string] $BaselineCommit, [Parameter(Mandatory)][string] $CandidateModulePath)
    $hash = { param([string] $Text) $sha=[Security.Cryptography.SHA256]::Create(); try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($Text -replace "`r`n","`n")))) } finally { $sha.Dispose() } }
    $candidateSource = [IO.File]::ReadAllText([IO.Path]::GetFullPath($CandidateModulePath)) -replace "`r`n", "`n"
    $candidateHash = & $hash $candidateSource
    $lines = @(& git -C $RepositoryRoot show "${BaselineCommit}:Tools/PerformanceComparison.psm1" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -eq 0) {
        return [ordered]@{ module=$null; status='policy-review-required'; reason='The measured base does not contain a readable immutable performance judge.'; baselineCommit=$BaselineCommit; baseJudgeSha256=$null; candidateJudgeSha256=$candidateHash; baseJudgeVersion=$null; candidateJudgeVersion=$null }
    }
    $source = ($lines -join "`n") + "`n"
    $baseHash = & $hash $source
    try {
        $candidateModule = New-Module -Name "DxUiCandidatePerformanceJudge_$([guid]::NewGuid().ToString('N'))" -ScriptBlock ([scriptblock]::Create($candidateSource))
        if (-not $candidateModule.ExportedFunctions.ContainsKey('Compare-PairedBlockSet')) { throw 'The candidate judge does not export Compare-PairedBlockSet.' }
        if (-not $candidateModule.ExportedFunctions.ContainsKey('Get-PairedPerformanceJudgeVersion')) { throw 'The candidate judge does not export its version identity.' }
        $module = New-Module -Name "DxUiBasePerformanceJudge_$([guid]::NewGuid().ToString('N'))" -ScriptBlock ([scriptblock]::Create($source))
        if (-not $module.ExportedFunctions.ContainsKey('Compare-PerformanceSet')) { throw 'The immutable base judge does not export Compare-PerformanceSet.' }
        $candidateVersion = & $candidateModule { Get-PairedPerformanceJudgeVersion }
        $baseVersion = if ($module.ExportedFunctions.ContainsKey('Get-PairedPerformanceJudgeVersion')) { & $module { Get-PairedPerformanceJudgeVersion } } else { 'legacy-unversioned' }
        return [ordered]@{ module=$module; candidateModule=$candidateModule; status='available'; reason='Measured-base and candidate judges loaded from their attested sources in isolated module scopes.'; baselineCommit=$BaselineCommit; baseJudgeSha256=$baseHash; candidateJudgeSha256=$candidateHash; baseJudgeVersion=$baseVersion; candidateJudgeVersion=$candidateVersion }
    }
    catch {
        return [ordered]@{ module=$null; status='policy-review-required'; reason="The immutable base judge could not be loaded: $($_.Exception.Message)"; baselineCommit=$BaselineCommit; baseJudgeSha256=$baseHash; candidateJudgeSha256=$candidateHash; baseJudgeVersion=$null; candidateJudgeVersion=$null }
    }
}

function Get-OverlayCompiledChanges {
    <# The compiled benchmark inputs an overlay changed. A tree whose existing build predates them was not built from the
       harness its receipts will name, so it must be rebuilt rather than reused. #>
    param([Parameter(Mandatory)][object[]] $Records)
    $compiled = @(Get-BenchmarkInputPaths)
    return @($Records | Where-Object { $_['action'] -cne 'unchanged' -and $_['path'] -cin $compiled } | ForEach-Object { $_['path'] })
}

Export-ModuleMember -Function Get-PairedHarness, Get-PairedSelection, Assert-PairedSidesDiffer, Assert-PairedTree, Copy-HarnessOverlay,
    Restore-HarnessOverlay, Get-PairedRunSchedule, Get-RandomizedPairedBlockSchedule, Get-VersionedPerformanceJudge, Get-OverlayCompiledChanges,
    New-PairedExecutionPlan
