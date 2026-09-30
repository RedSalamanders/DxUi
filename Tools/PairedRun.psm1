# Planning for performance-paired.ps1: which trees are measured, whether they differ, and how this checkout's harness
# reaches them. Decisions are pure and the file operations small, so the tooling tests drive them against fixture trees
# without a build. A side is an ordered dictionary: Role (baseline or candidate), Kind (revision, path or checkout), Spec
# (what the caller typed), and once resolved Root (the tree measured), Commit and Fingerprint.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1')

$script:PathComparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
# The measurement driver and its comparator; the compiled benchmark inputs come from the comparator module, which also
# hashes them into every receipt.
$script:HarnessScripts = @('performance.ps1', 'Tools/Compare-Performance.ps1', 'Tools/PerformanceComparison.psm1')

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
        foreach ($path in $Paths) {
            $from = Join-Path $Source $path
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

function Get-OverlayCompiledChanges {
    <# The compiled benchmark inputs an overlay changed. A tree whose existing build predates them was not built from the
       harness its receipts will name, so it must be rebuilt rather than reused. #>
    param([Parameter(Mandatory)][object[]] $Records)
    $compiled = @(Get-BenchmarkInputPaths)
    return @($Records | Where-Object { $_['action'] -cne 'unchanged' -and $_['path'] -cin $compiled } | ForEach-Object { $_['path'] })
}

Export-ModuleMember -Function Get-PairedHarness, Get-PairedSelection, Assert-PairedSidesDiffer, Assert-PairedTree, Copy-HarnessOverlay,
    Restore-HarnessOverlay, Get-OverlayCompiledChanges
