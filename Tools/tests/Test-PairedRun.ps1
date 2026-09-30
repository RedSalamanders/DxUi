# Which trees a paired run measures, when it refuses a pair, how the harness reaches a named tree and the run order.
# Everything here runs against fixture trees and dictionaries; no build or benchmark is needed.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PerformanceComparison.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../PairedRun.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Get-ErrorMessage([scriptblock] $Body) {
    try { & $Body } catch { return $_.Exception.Message }
    throw 'Expected an error.'
}

function Test-SameStamp([DateTime] $Expected, [string] $Path) {
    # File systems round timestamps; two seconds is coarser than any of them.
    return [Math]::Abs(([IO.File]::GetLastWriteTimeUtc($Path) - $Expected).TotalSeconds) -lt 2
}

function New-Side([string] $Kind, [string] $Commit, [string] $Root = '', [string] $Fingerprint = '') {
    $side = [ordered]@{ Kind = $Kind; Commit = $Commit }
    if ($Root) { $side['Root'] = $Root }
    if ($Fingerprint) { $side['Fingerprint'] = $Fingerprint }
    return $side
}

Invoke-TestCase 'the baseline and candidate are chosen by revision, named tree or checkout' {
    $selection = Get-PairedSelection -BaselineRevision 'abc123'
    Assert-Equal 'revision' $selection.Baseline.Kind 'baseline kind'
    Assert-Equal 'abc123' $selection.Baseline.Spec 'baseline revision'
    Assert-Equal 'checkout' $selection.Candidate.Kind 'the candidate defaults to this checkout'
    $selection = Get-PairedSelection -BaselinePath 'D:/trees/a' -CandidatePath 'D:/trees/b'
    Assert-Equal 'path' $selection.Baseline.Kind 'baseline tree'
    Assert-Equal 'path' $selection.Candidate.Kind 'candidate tree'
    Assert-Equal 'D:/trees/b' $selection.Candidate.Spec 'candidate tree text'
    $selection = Get-PairedSelection -BaselinePath 'D:/trees/a' -CandidateRevision 'def456'
    Assert-Equal 'revision' $selection.Candidate.Kind 'candidate revision beside a baseline tree'
    $selection = Get-PairedSelection -BaselineRevision 'abc123' -CandidateRevision '' -CandidatePath ''
    Assert-Equal 'checkout' $selection.Candidate.Kind 'empty inputs, as the hosted workflow passes them, are not given'
}

Invoke-TestCase 'an ambiguous or missing selection is rejected' {
    Assert-True (Get-ErrorMessage { Get-PairedSelection }).Contains('Name the baseline') 'no baseline'
    Assert-True (Get-ErrorMessage { Get-PairedSelection -BaselineRevision 'a' -BaselinePath 'b' }).Contains('not both') 'two baselines'
    Assert-True (Get-ErrorMessage { Get-PairedSelection -BaselineRevision 'a' -CandidateRevision 'b' -CandidatePath 'c' }).Contains('not both') 'two candidates'
    Assert-True (Get-ErrorMessage { Get-PairedSelection -CandidatePath 'c' }).Contains('Name the baseline') 'a candidate alone'
}

Invoke-TestCase 'revisions and this checkout are refused on one commit and only on the commit' {
    $message = Get-ErrorMessage { Assert-PairedSidesDiffer (New-Side 'revision' 'a1') (New-Side 'checkout' 'a1') }
    Assert-True $message.Contains('same commit') 'the commit rule'
    Assert-True $message.Contains('-CandidatePath') 'the refusal says how to measure uncommitted work'
    Assert-PairedSidesDiffer (New-Side 'revision' 'a1') (New-Side 'checkout' 'b2')
    Assert-PairedSidesDiffer (New-Side 'revision' 'a1') (New-Side 'revision' 'b2')
    # Identical sources on different commits (a documentation-only commit) stay measurable, as before.
    Assert-PairedSidesDiffer (New-Side 'revision' 'a1' -Fingerprint 'F') (New-Side 'checkout' 'b2' -Fingerprint 'F')
}

Invoke-TestCase 'a named tree is refused on identical library sources, not on one commit' {
    # Uncommitted work sits on the commit the baseline names; only its sources differ.
    Assert-PairedSidesDiffer (New-Side 'revision' 'a1' -Root 'D:/run/baseline' -Fingerprint 'AAAA') (New-Side 'path' 'a1' -Root 'D:/trees/work' -Fingerprint 'BBBB')
    Assert-PairedSidesDiffer (New-Side 'path' 'a1' -Root 'D:/trees/one' -Fingerprint 'AAAA') (New-Side 'path' 'a1' -Root 'D:/trees/two' -Fingerprint 'BBBB')
    $message = Get-ErrorMessage { Assert-PairedSidesDiffer (New-Side 'revision' 'a1' -Root 'D:/run/baseline' -Fingerprint 'AAAA') (New-Side 'path' 'b2' -Root 'D:/trees/work' -Fingerprint 'AAAA') }
    Assert-True $message.Contains('identical library sources') 'the fingerprint rule'
    Assert-True $message.Contains('AAAA') 'the refusal names the fingerprint'
    Assert-True (Get-ErrorMessage { Assert-PairedSidesDiffer (New-Side 'path' 'a1' -Root 'D:/trees/one' -Fingerprint 'AAAA') (New-Side 'checkout' 'a1' -Root 'D:/trees/two' -Fingerprint 'AAAA') }).Contains('identical library sources') 'a tree beside this checkout'
}

Invoke-TestCase 'a fingerprint that is not known yet defers the refusal, and the same tree is refused at once' {
    Assert-PairedSidesDiffer (New-Side 'revision' 'a1') (New-Side 'path' 'a1' -Root 'D:/trees/work')
    Assert-PairedSidesDiffer (New-Side 'path' 'a1' -Root 'D:/trees/one') (New-Side 'path' 'a1' -Root 'D:/trees/two')
    # Backslashes and case name one Windows directory; elsewhere they would name another.
    $spellings = @('D:/trees/one', 'D:/trees/one/') + $(if ($IsWindows) { 'D:\trees\one', 'd:/TREES/one' })
    foreach ($spelling in $spellings) {
        Assert-True (Get-ErrorMessage { Assert-PairedSidesDiffer (New-Side 'path' 'a1' -Root 'D:/trees/one') (New-Side 'path' 'b2' -Root $spelling) }).Contains('same tree') "the same tree spelled $spelling"
    }
}

Invoke-FixtureCase 'a named tree must be the top of a git working tree with the DxUi scripts' {
    param($root)
    Assert-Throws { Assert-PairedTree (Join-Path $root 'nothing') } 'a directory that does not exist'
    $tree = Join-Path $root 'tree'
    New-Item -ItemType Directory -Path $tree | Out-Null
    # The fixture root lies inside this checkout, so a plain directory belongs to the outer repository, not to itself.
    Assert-Throws { Assert-PairedTree $tree } 'a directory that is not a working tree of its own'
    & git -C $tree init --quiet
    Assert-Equal 0 $LASTEXITCODE 'git init'
    Assert-Throws { Assert-PairedTree $tree } 'a repository without the DxUi scripts'
    foreach ($name in @('build.ps1', 'vcpkg-install.ps1')) { Set-FixtureFile $tree $name '' }
    foreach ($name in @('src', 'include')) { New-Item -ItemType Directory -Path (Join-Path $tree $name) | Out-Null }
    Assert-Equal ([IO.Path]::GetFullPath($tree)) (Assert-PairedTree $tree) 'a DxUi working tree'
    Assert-Throws { Assert-PairedTree (Join-Path $tree 'src') } 'a folder inside a working tree'
}

Invoke-FixtureCase 'the harness overlay leaves identical files alone, stamps what it writes and restores the tree' {
    param($root)
    $source = Join-Path $root 'harness'
    $target = Join-Path $root 'tree'
    $backup = Join-Path $root 'backup'
    $paths = @('performance.ps1', 'Tools/Compare-Performance.ps1', 'Tests/Embedded/Bench.h')
    foreach ($path in $paths) { Set-FixtureFile $source $path "harness $path`n" }
    Set-FixtureFile $target 'performance.ps1' "harness performance.ps1`n"
    Set-FixtureFile $target 'Tools/Compare-Performance.ps1' "older`n"
    Set-FixtureFile $target 'unrelated.txt' "kept`n"
    # The source files are old; a copy that kept their stamp would look older than the tree's build outputs.
    $past = [DateTime]::UtcNow.AddYears(-1)
    foreach ($path in $paths) { [IO.File]::SetLastWriteTimeUtc((Join-Path $source $path), $past) }
    [IO.File]::SetLastWriteTimeUtc((Join-Path $target 'performance.ps1'), $past)

    $records = @(Copy-HarnessOverlay -Source $source -Target $target -Paths $paths -BackupDirectory $backup)
    Assert-Equal 'unchanged replaced created' (($records | ForEach-Object { $_.action }) -join ' ') 'actions'
    Assert-Equal "harness Tools/Compare-Performance.ps1`n" ([IO.File]::ReadAllText((Join-Path $target 'Tools/Compare-Performance.ps1'))) 'a replaced file'
    Assert-Equal "harness Tests/Embedded/Bench.h`n" ([IO.File]::ReadAllText((Join-Path $target 'Tests/Embedded/Bench.h'))) 'a created file'
    Assert-True (Test-SameStamp $past (Join-Path $target 'performance.ps1')) 'an identical file keeps its stamp'
    foreach ($path in @('Tools/Compare-Performance.ps1', 'Tests/Embedded/Bench.h')) {
        Assert-True ([IO.File]::GetLastWriteTimeUtc((Join-Path $target $path)) -gt $past.AddMonths(6)) "$path is stamped with the current time"
    }
    Assert-Equal "older`n" ([IO.File]::ReadAllText((Join-Path $backup 'Tools/Compare-Performance.ps1'))) 'the replaced file is backed up'
    Assert-Equal 2 @($records[2].createdDirectories).Count 'directories created for the new file'
    Assert-True ([string]$records[2].createdDirectories[0]).EndsWith('Tests') 'outermost first'

    Restore-HarnessOverlay -Target $target -Records $records -BackupDirectory $backup
    Assert-Equal "older`n" ([IO.File]::ReadAllText((Join-Path $target 'Tools/Compare-Performance.ps1'))) 'the replaced file returns'
    Assert-True ([IO.File]::GetLastWriteTimeUtc((Join-Path $target 'Tools/Compare-Performance.ps1')) -gt $past.AddMonths(6)) 'a restored file is stamped so the next build sees it'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $target 'Tests'))) 'the created file and its directories are gone'
    Assert-True (Test-Path -LiteralPath (Join-Path $target 'unrelated.txt')) 'unrelated files stay'
    Assert-True (Test-SameStamp $past (Join-Path $target 'performance.ps1')) 'an untouched file keeps its stamp through the restore'
}

Invoke-FixtureCase 'a restore keeps a directory that gained other files' {
    param($root)
    $source = Join-Path $root 'harness'
    $target = Join-Path $root 'tree'
    Set-FixtureFile $source 'Tools/Compare-Performance.ps1' "harness`n"
    $records = @(Copy-HarnessOverlay -Source $source -Target $target -Paths @('Tools/Compare-Performance.ps1') -BackupDirectory (Join-Path $root 'backup'))
    Set-FixtureFile $target 'Tools/added-later.txt' "not the overlay's`n"
    Restore-HarnessOverlay -Target $target -Records $records -BackupDirectory (Join-Path $root 'backup')
    Assert-True (Test-Path -LiteralPath (Join-Path $target 'Tools/added-later.txt')) 'the later file survives'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $target 'Tools/Compare-Performance.ps1'))) 'the overlay file is gone'
}

Invoke-FixtureCase 'a tree that is the harness source is never rewritten' {
    param($root)
    $source = Join-Path $root 'harness'
    Set-FixtureFile $source 'performance.ps1' "harness`n"
    foreach ($target in @($source, "$source/") + $(if ($IsWindows) { $source.ToUpperInvariant() })) {
        $records = @(Copy-HarnessOverlay -Source $source -Target $target -Paths @('performance.ps1'))
        Assert-Equal 'unchanged' $records[0].action "the same tree spelled $target"
    }
}

Invoke-FixtureCase 'a failed overlay leaves the tree as it was found' {
    param($root)
    $source = Join-Path $root 'harness'
    $target = Join-Path $root 'tree'
    $backup = Join-Path $root 'backup'
    Set-FixtureFile $source 'Tools/Compare-Performance.ps1' "harness`n"
    Set-FixtureFile $source 'Tests/Embedded/Bench.h' "harness`n"
    Set-FixtureFile $target 'Tools/Compare-Performance.ps1' "older`n"
    $message = Get-ErrorMessage { Copy-HarnessOverlay -Source $source -Target $target -Paths @('Tools/Compare-Performance.ps1', 'Tests/Embedded/Bench.h', 'Missing/Input.h') -BackupDirectory $backup }
    Assert-True $message.Contains('Missing benchmark harness input: Missing/Input.h') 'the missing input is named'
    Assert-Equal "older`n" ([IO.File]::ReadAllText((Join-Path $target 'Tools/Compare-Performance.ps1'))) 'the replaced file is back'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $target 'Tests'))) 'the created file is gone'
}

Invoke-TestCase 'one repetition is the single pass a set has always been' {
    $schedule = Get-PairedRunSchedule -Repetitions 1
    Assert-Equal 'A1 B1 B2 A2' (($schedule.Steps | ForEach-Object { $_.Name }) -join ' ') 'run names'
    Assert-Equal 'baseline candidate candidate baseline' (($schedule.Steps | ForEach-Object { $_.Side }) -join ' ') 'trees measured'
    Assert-Equal 'A1, B1, B2, A2' $schedule.Order 'order text'
    Assert-Equal 'B1-vs-A1 B2-vs-A2 A2-vs-A1-control B2-vs-B1-control' (($schedule.Comparisons | ForEach-Object { $_.Name }) -join ' ') 'comparison files'
    Assert-Equal 'B1/A1 B2/A2 A2/A1 B2/B1' (($schedule.Comparisons | ForEach-Object { "$($_.Candidate)/$($_.Baseline)" }) -join ' ') 'candidate against baseline'
    Assert-Equal 'False False True True' (($schedule.Comparisons | ForEach-Object { $_.Control }) -join ' ') 'controls'
}

Invoke-TestCase 'repetitions repeat the interleaved pass and keep every pass its own comparisons' {
    $schedule = Get-PairedRunSchedule
    $names = @($schedule.Steps | ForEach-Object { $_.Name })
    Assert-Equal 'A1 B1 B2 A2 A3 B3 B4 A4 A5 B5 B6 A6' ($names -join ' ') 'three repetitions by default'
    Assert-Equal 'A1, B1, B2, A2, A3, B3, B4, A4, A5, B5, B6, A6' $schedule.Order 'order text'
    Assert-Equal 6 @($schedule.Steps | Where-Object { $_.Side -eq 'baseline' }).Count 'runs of the baseline'
    Assert-Equal 6 @($schedule.Steps | Where-Object { $_.Side -eq 'candidate' }).Count 'runs of the candidate'
    Assert-Equal 12 @($schedule.Comparisons).Count 'two crossings and two controls per pass'
    Assert-Equal 12 @($schedule.Comparisons | ForEach-Object { $_.Name } | Select-Object -Unique).Count 'no comparison file is written twice'
    $first = @((Get-PairedRunSchedule -Repetitions 1).Comparisons | ForEach-Object { $_.Name })
    Assert-Equal ($first -join ' ') (@($schedule.Comparisons | Select-Object -First 4 | ForEach-Object { $_.Name }) -join ' ') 'the first pass keeps its names'
    foreach ($comparison in $schedule.Comparisons) {
        Assert-Contains $names $comparison.Candidate "a run measured for $($comparison.Name)"
        Assert-Contains $names $comparison.Baseline "a run measured for $($comparison.Name)"
    }
    # A linear drift cancels within every pass: the baseline's two run positions sum to the candidate's.
    for ($pass = 0; $pass -lt 3; $pass++) {
        $steps = @($schedule.Steps | Select-Object -Skip (4 * $pass) -First 4)
        $baselineSum = 0
        $candidateSum = 0
        for ($position = 0; $position -lt 4; $position++) { if ($steps[$position].Side -eq 'baseline') { $baselineSum += $position } else { $candidateSum += $position } }
        Assert-Equal $baselineSum $candidateSum "pass $($pass + 1) is balanced against linear drift"
    }
    Assert-Equal 40 @((Get-PairedRunSchedule -Repetitions 10).Steps).Count 'ten repetitions'
    Assert-Throws { Get-PairedRunSchedule -Repetitions 0 } 'no repetitions'
    Assert-Throws { Get-PairedRunSchedule -Repetitions 11 } 'more than ten repetitions'
}

Invoke-TestCase 'only an overlay that changed a compiled input forbids reusing a build' {
    $records = @(
        [ordered]@{ path = 'performance.ps1'; action = 'replaced' }
        [ordered]@{ path = 'Tests/Embedded/ComplexUiBenchmark.h'; action = 'replaced' }
        [ordered]@{ path = 'Samples/ComplexUi/ComplexUiScene.h'; action = 'created' }
        [ordered]@{ path = 'Tests/Embedded/BenchmarkMain.h'; action = 'unchanged' })
    Assert-Equal 'Tests/Embedded/ComplexUiBenchmark.h Samples/ComplexUi/ComplexUiScene.h' (@(Get-OverlayCompiledChanges $records) -join ' ') 'compiled changes'
    Assert-Equal 0 @(Get-OverlayCompiledChanges @($records[0], $records[3])).Count 'scripts and unchanged inputs need no rebuild'
}

Invoke-TestCase 'the harness is the driver, the comparator and every benchmark input, all present' {
    $harness = @(Get-PairedHarness)
    foreach ($required in @('performance.ps1', 'Tools/Compare-Performance.ps1', 'Tools/PerformanceComparison.psm1') + @(Get-BenchmarkInputPaths)) {
        Assert-Contains $harness $required 'the harness list'
    }
    Assert-Equal $harness.Count @($harness | Select-Object -Unique).Count 'no duplicates'
    foreach ($path in $harness) { Assert-True (Test-Path -LiteralPath (Join-Path $repository $path) -PathType Leaf) "the harness file exists: $path" }
}

function Get-HistoricFingerprint([string] $Root) {
    # performance.ps1's own computation before it moved into the comparator module, verbatim, run from the tree's root.
    Push-Location $Root
    try {
        $sources = @(& git ls-files --cached --others --exclude-standard -- src include Build Directory.Build.props Directory.Build.targets vcpkg.json vcpkg-tool.json)
        $hashes = foreach ($source in ($sources | Sort-Object -Unique)) { "$source $((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash)" }
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($hashes -join "`n")))) } finally { $sha.Dispose() }
    } finally { Pop-Location }
}

Invoke-FixtureCase 'the source fingerprint is the one receipts have always recorded, over the library inputs only' {
    param($root)
    $first = Join-Path $root 'first'
    $second = Join-Path $root 'second'
    foreach ($tree in @($first, $second)) {
        New-Item -ItemType Directory -Path $tree | Out-Null
        & git -C $tree init --quiet
        Set-FixtureFile $tree '.gitignore' "src/generated.cpp`n"
        Set-FixtureFile $tree 'src/Controls/Control.cpp' "one`n"
        Set-FixtureFile $tree 'src/Controls/Grid_Model.cpp' "two`n"
        Set-FixtureFile $tree 'src/Controls/GridView.cpp' "three`n"
        Set-FixtureFile $tree 'include/DxUi/Control.h' "four`n"
        Set-FixtureFile $tree 'vcpkg.json' "{}`n"
        Set-FixtureFile $tree 'docs/note.md' "prose`n"
    }
    $fingerprint = Get-SourceFingerprint -Root $first
    Assert-True ($fingerprint -cmatch '^[0-9A-F]{64}$') 'a SHA-256 in upper case'
    Assert-Equal (Get-HistoricFingerprint $first) $fingerprint 'the historic computation'
    Assert-Equal $fingerprint (Get-SourceFingerprint -Root $second) 'two trees with the same library inputs share a fingerprint'
    & git -C $first -c core.autocrlf=false add -A
    Assert-Equal $fingerprint (Get-SourceFingerprint -Root $first) 'tracked and untracked inputs count alike'
    Set-FixtureFile $first 'docs/note.md' "changed prose`n"
    Set-FixtureFile $first 'src/generated.cpp' "ignored`n"
    Assert-Equal $fingerprint (Get-SourceFingerprint -Root $first) 'documentation and ignored files are not library inputs'
    Set-FixtureFile $first 'include/DxUi/New.h' "five`n"
    $changed = Get-SourceFingerprint -Root $first
    Assert-True ($changed -cne $fingerprint) 'a new library input changes it'
    Assert-Equal (Get-HistoricFingerprint $first) $changed 'the historic computation, after the change'
    Assert-True ((Get-SourceFingerprint -Root $second) -ceq $fingerprint) 'the other tree is unaffected'
    Set-FixtureFile $second 'src/Controls/Control.cpp' "edited`n"
    Assert-True ((Get-SourceFingerprint -Root $second) -cne $fingerprint) 'an edit changes it'
}

Invoke-TestCase 'the measurement scripts parse' {
    foreach ($name in @('performance.ps1', 'performance-paired.ps1')) {
        $tokens = $null
        $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $repository $name), [ref]$tokens, [ref]$errors)
        Assert-Equal 0 @($errors).Count "$name has no syntax errors: $(@($errors | ForEach-Object { $_.Message }) -join '; ')"
    }
}

Complete-TestRun 'PairedRun'
