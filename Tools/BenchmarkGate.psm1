# The gate around the hosted paired benchmark: which pull requests it runs for, what it compares, and how a set's
# verdict becomes the check's conclusion and its job summary. The measurement stays in performance-paired.ps1 and
# PerformanceComparison.psm1; this module only decides, from files and dictionaries, so the tooling tests drive every
# decision without a build or a runner.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1')
Import-Module (Join-Path $PSScriptRoot 'PairedRun.psm1')

$script:Invariant = [Globalization.CultureInfo]::InvariantCulture
# The scenarios the contract names for a grid text change and the default benchmark, and the repetitions its paired-set
# rule needs: three give six runs per side, whose smallest attainable p is 0.0022; two give 0.029 and one cannot reach 0.05.
$script:GateScenarios = @('Default', 'MultilineGrid', 'MultilineGridDistinct')
$script:GateRepetitions = 3
# The significance level of Compare-PerformanceSet; a set whose smallest attainable p is not below it cannot judge a timing.
$script:Significance = 0.05

function Get-BenchmarkGateScenarios { return @($script:GateScenarios) }

function Get-BenchmarkGateRepetitions { return $script:GateRepetitions }

# --- Which pull requests are measured -----------------------------------------------------------------------------------

function Format-RepositoryPath {
    <# A repository-relative path with forward slashes and no leading ./, the spelling git prints. #>
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Path)
    $normal = $Path.Replace('\', '/')
    while ($normal.StartsWith('./', [StringComparison]::Ordinal)) { $normal = $normal.Substring(2) }
    return $normal
}

function Get-BenchmarkScopeRules {
    <# What can change a hosted paired run's result: a reason for the summary and the paths that carry it. A path is a file
       or a directory (it covers everything beneath it). The library inputs and the harness come from the modules that hash
       them into every receipt, so this list cannot drift from what a receipt covers. #>
    return @(
        [ordered]@{ Reason = 'library input'; Paths = @(Get-LibraryInputPaths) }
        [ordered]@{ Reason = 'benchmark harness'; Paths = @(Get-PairedHarness) }
        [ordered]@{ Reason = 'benchmark executable'; Paths = @('Tests/Embedded', 'Tests/Support') }
        [ordered]@{ Reason = 'fixture or sample compiled into it'; Paths = @('Samples') }
        [ordered]@{ Reason = 'build or restore'; Paths = @('DxUi.sln', 'build.ps1', 'vcpkg-install.ps1', 'vcpkg-configuration.json', 'Tools/VisualStudio.psm1', 'Tools/VcpkgTriplet.psm1') }
        [ordered]@{ Reason = 'paired measurement or gate'; Paths = @('performance-paired.ps1', 'Tools/PairedRun.psm1', 'Tools/BenchmarkGate.psm1', 'Tools/Get-BenchmarkScope.ps1', 'Tools/Publish-BenchmarkVerdict.ps1') }
        [ordered]@{ Reason = 'hosted workflow'; Paths = @('.github/workflows/ci.yml') }
    )
}

function Get-BenchmarkScope {
    <# Whether a change can alter what the paired benchmark measures, and which of its files say so. Anything not named by
       Get-BenchmarkScopeRules is not measured, and Markdown never is: documentation, the specifications, retained
       measurements, changelog fragments, the control and foundation tests and every other tool leave the benchmark's
       executable, fixture and verdict as they were. #>
    param([Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][AllowNull()][string[]] $ChangedPaths)
    $rules = @(Get-BenchmarkScopeRules)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $hits = [Collections.Generic.List[object]]::new()
    foreach ($changed in @($ChangedPaths | Where-Object { $_ })) {
        $path = Format-RepositoryPath $changed
        if (-not $seen.Add($path)) { continue }
        if ($path.EndsWith('.md', [StringComparison]::Ordinal)) { continue }
        foreach ($rule in $rules) {
            $covered = @($rule['Paths'] | Where-Object { $path -ceq $_ -or $path.StartsWith("$_/", [StringComparison]::Ordinal) }).Count -gt 0
            if ($covered) { $hits.Add([ordered]@{ Path = $path; Reason = $rule['Reason'] }); break }
        }
    }
    return [ordered]@{ Relevant = ($hits.Count -gt 0); Total = $seen.Count; Ignored = $seen.Count - $hits.Count; Matches = $hits.ToArray() }
}

function Invoke-GateGit {
    <# The lines git printed. Its warnings on stderr are not among them, but a failure carries them. #>
    param([Parameter(Mandatory)][string] $Repository, [Parameter(Mandatory)][string[]] $Arguments)
    $all = @(& git -C $Repository @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in ${Repository}: $(@($all | ForEach-Object { "$_" }) -join ' ')" }
    return @($all | Where-Object { $_ -is [string] })
}

function Resolve-PullRequestPair {
    <# The two commits a pull request's paired benchmark compares. The candidate is the checkout (GitHub's merge ref of the
       pull request into its base). The baseline is the merge commit's first parent: the base branch as the merge ref was
       made, so baseline and candidate differ by exactly this pull request. The merge base with the pull request's head
       would also contain whatever the base gained since the branch was cut, and a regression it brought would be blamed on
       this pull request; the two agree when the base has not moved. A checkout that is not a merge commit has no such
       parent, and the merge base with origin/<BaseRef> stands in. #>
    param([string] $Repository = '.', [string] $Candidate = 'HEAD', [Parameter(Mandatory)][string] $BaseRef)
    $commit = @(Invoke-GateGit $Repository @('rev-parse', '--verify', '--quiet', "$Candidate^{commit}"))[0].Trim()
    $line = @(Invoke-GateGit $Repository @('rev-list', '--parents', '-n', '1', $commit))[0].Trim()
    $parents = @($line -split '\s+' | Select-Object -Skip 1)
    if ($parents.Count -ge 2) {
        return [ordered]@{ Candidate = $commit; Baseline = $parents[0]; BaseRef = $BaseRef; Method = 'first parent of the merge commit (the base as the merge ref was made)' }
    }
    $base = @(Invoke-GateGit $Repository @('merge-base', $commit, "origin/$BaseRef"))[0].Trim()
    return [ordered]@{ Candidate = $commit; Baseline = $base; BaseRef = $BaseRef; Method = "merge base with origin/$BaseRef (the checkout is not a merge commit)" }
}

function Get-PullRequestChangedPaths {
    <# Every path a pull request changes, as git names it. Renames are not detected, so a file moved out of a measured
       directory is listed under its old name as well as its new one. #>
    param([string] $Repository = '.', [Parameter(Mandatory)][string] $Baseline, [Parameter(Mandatory)][string] $Candidate)
    return @(Invoke-GateGit $Repository @('-c', 'core.quotepath=off', 'diff', '--name-only', '--no-renames', $Baseline, $Candidate) | Where-Object { $_ })
}

# --- Control drift --------------------------------------------------------------------------------------------------------

function Read-GateJson {
    param([Parameter(Mandatory)][string] $Path)
    $text = [IO.File]::ReadAllText([IO.Path]::GetFullPath($Path), [Text.UTF8Encoding]::new($false, $true))
    return $text | ConvertFrom-Json -AsHashtable
}

function Test-ControlName([string] $Name) { return $Name.EndsWith('-control', [StringComparison]::Ordinal) }

function Get-ControlDrift {
    <# What a scenario's same-binary controls (A2 against A1 and B2 against B1 of every pass) did to each metric. A control
       measures one binary twice, so any change in it is the machine's. A metric has drifted when a control moved it beyond
       its investigation band in either direction (an exact budget: at all), and is held when every control saw it and
       none drifted. A control whose comparison cannot be read saw nothing, so the metrics of such a scenario are not held. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Scenario, [Parameter(Mandatory)][string] $ReportsDirectory)
    $controls = @($Scenario['comparisons'] | Where-Object { $null -ne $_ -and (Test-ControlName ([string]$_['name'])) })
    $metrics = @{}
    $missing = 0
    $unstable = 0
    foreach ($control in $controls) {
        if ($control['status'] -ceq 'unstable-control') { $unstable++ }
        $changes = $null
        $file = if ($control['file']) { Join-Path $ReportsDirectory ([string]$control['file']) } else { $null }
        if ($file -and (Test-Path -LiteralPath $file -PathType Leaf)) {
            try { $changes = @((Read-GateJson $file)['changes']) } catch { $changes = $null }
        }
        if ($null -eq $changes -or $changes.Count -eq 0) { $missing++; continue }
        foreach ($change in $changes) {
            $key = '{0}/{1}' -f $change['scenario'], $change['metric']
            if (-not $metrics.ContainsKey($key)) { $metrics[$key] = [ordered]@{ Observed = 0; Drifted = $false; MaxDriftPercent = 0.0 } }
            $record = $metrics[$key]
            $record['Observed']++
            if ($null -eq $change['changePercent']) {
                if (-not (Test-SameNumber $change['before'] $change['after'])) { $record['Drifted'] = $true }
                continue
            }
            $percent = [Math]::Abs([double]$change['changePercent'])
            if ($percent -gt $record['MaxDriftPercent']) { $record['MaxDriftPercent'] = $percent }
            if ($percent -gt [double]$change['noisePercent']) { $record['Drifted'] = $true }
        }
    }
    return [ordered]@{ Total = $controls.Count; Unstable = $unstable; Missing = $missing; Metrics = $metrics }
}

function Test-SameNumber([object] $Left, [object] $Right) {
    return $null -ne $Left -and $null -ne $Right -and [double]$Left -eq [double]$Right
}

# --- Verdict to conclusion ------------------------------------------------------------------------------------------------

# Worst first: the conclusion of a run is the worst conclusion of its scenarios.
$script:ConclusionOrder = @('invalid', 'degraded', 'inconclusive', 'pass')

function Get-WorstConclusion([string[]] $Conclusions) {
    foreach ($name in $script:ConclusionOrder) { if ($Conclusions -ccontains $name) { return $name } }
    return 'invalid'
}

function New-ScenarioResult([string] $Scenario, [string] $SetStatus = '') {
    return [ordered]@{
        Scenario = $Scenario; SetStatus = $SetStatus; Conclusion = 'invalid'; Notes = [string[]]@()
        Regressed = 0; Improved = 0; Judged = 0; Held = 0; Controls = [ordered]@{ Total = 0; Unstable = 0; Missing = 0 }
        MinimumAttainableP = $null; Metrics = @()
    }
}

function Get-ScenarioConclusion {
    <# One scenario of a retained summary: its set verdict, read with its same-binary controls. A scenario whose summary does not
       hold what this needs (it predates the paired sets, or its comparison did not run) is invalid, never an exception. #>
    param([AllowNull()][object] $Entry, [Parameter(Mandatory)][string] $ReportsDirectory, [bool] $Unchanged, [bool] $Strict)
    $isEntry = $Entry -is [System.Collections.IDictionary]
    $set = if ($isEntry) { $Entry['set'] } else { $null }
    $isSet = $set -is [System.Collections.IDictionary]
    $result = New-ScenarioResult $(if ($isEntry) { [string]$Entry['scenario'] } else { '' }) $(if ($isSet) { [string]$set['status'] } else { '' })
    if (-not $isSet) {
        $result['Notes'] = [string[]]@('Invalid evidence: the summary holds no set verdict for this scenario (it predates the paired sets, or its comparison did not run).')
        return $result
    }
    $notes = [Collections.Generic.List[string]]::new()
    if ($set['status'] -ceq 'invalid-evidence' -or $set['status'] -cnotin @('advice-required', 'within-noise-budget')) {
        $notes.Add("Invalid evidence: $($set['error'])")
        $result['Notes'] = $notes.ToArray()
        return $result
    }
    $drift = Get-ControlDrift -Scenario $Entry -ReportsDirectory $ReportsDirectory
    $result['Controls'] = [ordered]@{ Total = $drift['Total']; Unstable = $drift['Unstable']; Missing = $drift['Missing'] }
    $result['MinimumAttainableP'] = $set['minimumAttainableP']
    $metrics = [Collections.Generic.List[object]]::new()
    foreach ($metric in @($set['metrics'])) {
        $key = '{0}/{1}' -f $metric['phase'], $metric['metric']
        $seen = $drift['Metrics'][$key]
        # Held: every control measured this metric and none moved it beyond its band. A control whose comparison cannot be
        # read measured nothing, so no metric of its scenario is observed by all of them; without controls nothing is observed.
        $held = $null -ne $seen -and $seen['Observed'] -eq $drift['Total'] -and -not $seen['Drifted']
        # An exact budget has a band of zero; a summary that does not say so is still read by its band.
        $exact = [bool]$metric['exact'] -or ($null -ne $metric['noisePercent'] -and [double]$metric['noisePercent'] -eq 0)
        $outcome = switch ($metric['verdict']) {
            'regressed' { if ($Unchanged -and -not $exact) { 'noise' } elseif ($held) { 'confirmed' } else { 'unconfirmed' } }
            'improved' { 'improved' }
            default { 'clear' }
        }
        $metrics.Add([ordered]@{
                Phase = [string]$metric['phase']; Metric = [string]$metric['metric']; Exact = $exact; Verdict = [string]$metric['verdict']
                Outcome = $outcome; Held = [bool]$held; ControlDriftPercent = $(if ($null -ne $seen) { $seen['MaxDriftPercent'] } else { $null })
                ControlDrifted = $(if ($null -ne $seen) { [bool]$seen['Drifted'] } else { $null })
                BaselineMedian = $metric['baselineMedian']; CandidateMedian = $metric['candidateMedian']; ChangePercent = $metric['changePercent']
                NoisePercent = $metric['noisePercent']; PValue = $metric['pValue']
                BaselineSpreadPercent = $metric['baselineSpreadPercent']; CandidateSpreadPercent = $metric['candidateSpreadPercent']
            })
    }
    $result['Metrics'] = $metrics.ToArray()
    $result['Judged'] = $metrics.Count
    $result['Held'] = @($metrics | Where-Object { $_['Held'] }).Count
    $result['Regressed'] = @($metrics | Where-Object { $_['Verdict'] -ceq 'regressed' }).Count
    $result['Improved'] = @($metrics | Where-Object { $_['Verdict'] -ceq 'improved' }).Count
    $confirmed = @($metrics | Where-Object { $_['Outcome'] -ceq 'confirmed' }).Count
    $unconfirmed = @($metrics | Where-Object { $_['Outcome'] -ceq 'unconfirmed' }).Count
    $conclusion = if ($confirmed) { 'degraded' } elseif ($unconfirmed) { 'inconclusive' } else { 'pass' }
    if ($unconfirmed -and -not $confirmed) {
        $notes.Add("$unconfirmed regressed $(if ($unconfirmed -eq 1) { 'metric has same-binary controls that drifted' } else { 'metrics have same-binary controls that drifted' }) beyond their band on this runner, so the flag cannot be told from machine noise. Re-run the job; the flagged metrics are listed below, not dismissed.")
    }
    if ($conclusion -ceq 'pass' -and $null -ne $set['minimumAttainableP'] -and [double]$set['minimumAttainableP'] -ge $script:Significance) {
        $conclusion = 'inconclusive'
        $notes.Add("The set has $($set['baselineRuns']) runs against $($set['candidateRuns']), whose smallest attainable p is $(([double]$set['minimumAttainableP']).ToString('0.####', $script:Invariant)): too few to reach p < 0.05, so only a rise in an exact budget could be established. Repeat with more repetitions.")
    }
    if ($Strict -and $drift['Unstable'] -gt 0 -and $conclusion -cin @('pass', 'degraded')) {
        $conclusion = 'inconclusive'
        $notes.Add("Strict controls: $($drift['Unstable']) of $($drift['Total']) same-binary controls drifted beyond a band, so the runner was not held still.")
    }
    if (@($metrics | Where-Object { $_['Outcome'] -ceq 'noise' }).Count) {
        $notes.Add('The compiled library inputs of both sides are identical, so the timing and memory flags marked noise were measured on the same library code twice; they are chance. Deterministic budgets still gate.')
    }
    $result['Conclusion'] = $conclusion
    $result['Notes'] = $notes.ToArray()
    return $result
}

function Get-BenchmarkConclusion {
    <# The check's conclusion for one retained paired run: pass, inconclusive, degraded or invalid.

       A set's verdict (Compare-PerformanceSet) says which metrics regressed against the rank test and the bands. The check
       asks one thing more of a regressed metric before it fails a pull request: that the machine held still for it. A
       regressed metric whose same-binary controls (Get-ControlDrift) all stayed inside its band is confirmed; one whose
       controls drifted beyond the band cannot be told from the runner and leaves the run inconclusive. It is never
       dropped: every flagged metric is listed with its drift. Judging the controls of the metric in question, not of all
       twenty-six, is deliberate: on a shared hosted runner some of them (mostly the p95 of frame, preparation and
       composition times, and the working set) drift beyond their bands in nearly every control, so one drifting metric would
       make every run inconclusive. StrictControls restores the all-metrics reading: any unstable control makes the scenario
       inconclusive, whatever it found.

       Two more rules keep a pass honest. A set whose smallest attainable p is not below 0.05 can establish only a rise in an
       exact budget, so its pass is inconclusive. And when the compiled library inputs of both sides are identical, a timing
       or memory flag can only be the machine (both sides ran the same library code), so it is listed as noise; a rise in a
       deterministic budget is still judged, because noise cannot move one. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Summary, [Parameter(Mandatory)][string] $ReportsDirectory, [switch] $StrictControls)
    $baselineFingerprint = if ($Summary['baseline'] -is [System.Collections.IDictionary]) { [string]$Summary['baseline']['sourceFingerprint'] } else { '' }
    $candidateFingerprint = if ($Summary['candidate'] -is [System.Collections.IDictionary]) { [string]$Summary['candidate']['sourceFingerprint'] } else { '' }
    $unchanged = [bool]$baselineFingerprint -and [string]::Equals($baselineFingerprint, $candidateFingerprint, [StringComparison]::Ordinal)
    $scenarios = [Collections.Generic.List[object]]::new()
    foreach ($entry in @($Summary['scenarios'] | Where-Object { $null -ne $_ })) {
        try { $scenarios.Add((Get-ScenarioConclusion -Entry $entry -ReportsDirectory $ReportsDirectory -Unchanged $unchanged -Strict ([bool]$StrictControls))) }
        catch {
            $failed = New-ScenarioResult $(if ($entry -is [System.Collections.IDictionary]) { [string]$entry['scenario'] } else { '' })
            $failed['Notes'] = [string[]]@("Invalid evidence: the scenario could not be read: $($_.Exception.Message)")
            $scenarios.Add($failed)
        }
    }
    # A run is only as good as its scenarios, and a summary that lists none judged nothing: it says so itself, as there is no scenario to.
    $notes = [string[]]@(if ($scenarios.Count -eq 0) { 'Invalid evidence: the summary lists no scenario, so there is nothing to judge.' })
    $overall = if ($scenarios.Count) { Get-WorstConclusion @($scenarios | ForEach-Object { $_['Conclusion'] }) } else { 'invalid' }
    return [ordered]@{ Conclusion = $overall; StrictControls = [bool]$StrictControls; LibraryUnchanged = $unchanged; Notes = $notes; Scenarios = $scenarios.ToArray() }
}

# --- What a run says ------------------------------------------------------------------------------------------------------

function Get-BenchmarkHeadline {
    param([Parameter(Mandatory)][string] $Conclusion)
    switch ($Conclusion) {
        'pass' { return 'No regression established' }
        'degraded' { return 'Confirmed degradation' }
        'inconclusive' { return 'Inconclusive' }
        default { return 'Invalid evidence' }
    }
}

function Get-BenchmarkMeaning {
    <# The one paragraph that says what a conclusion means and what to do about it. #>
    param([Parameter(Mandatory)][string] $Conclusion)
    switch ($Conclusion) {
        'pass' { return 'No metric regressed. The contract reads this as no change established, which is not evidence that none exists; metrics whose same-binary controls drifted could not resolve their band on this runner and are counted below.' }
        'degraded' { return 'A metric regressed (exact rank test against its investigation band, or a rise in an exact budget) and its same-binary controls stayed inside the band. The contract stops the affected development: present the deltas and the suspected cause to the developer and choose among optimizing the path, reducing optional scope or deferring or reverting the change. Never relax a band or replace the baseline to pass.' }
        'inconclusive' { return 'The run cannot say. A metric was flagged but its same-binary controls drifted beyond its band, or the set is too small to judge a timing. It does not pass: re-run the job for a new runner, or repeat the set on a quiet machine, and treat the flagged metrics as unresolved findings, not as noise.' }
        default { return 'The run produced no usable verdict (a receipt failed its checks, the fixtures differ or the run did not finish). It proves nothing about performance; see the log.' }
    }
}

function Format-GateValue {
    param([Parameter(Mandatory)][string] $Metric, [object] $Value)
    if ($null -eq $Value) { return 'n/a' }
    $number = [double]$Value
    if ($Metric -ceq 'fps') { return $number.ToString('N1', $script:Invariant) }
    if ($Metric.EndsWith('Ms', [StringComparison]::Ordinal)) { return $number.ToString('0.000#', $script:Invariant) }
    return $number.ToString('N0', $script:Invariant)
}

function Format-GatePercent {
    param([object] $Value, [string] $Format = '+0.00;-0.00;0.00')
    if ($null -eq $Value) { return 'n/a' }
    return ([double]$Value).ToString($Format, $script:Invariant) + '%'
}

function Format-GateP([object] $Value) {
    if ($null -eq $Value) { return 'n/a' }
    return ([double]$Value).ToString('0.0000', $script:Invariant)
}

function ConvertTo-MarkdownCell([AllowNull()][object] $Text) {
    return ([string]$Text).Replace('|', '\|').Replace("`r", ' ').Replace("`n", ' ')
}

function Get-OutcomeText {
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Metric)
    switch ($Metric['Outcome']) {
        'confirmed' { return 'regressed, confirmed' }
        'unconfirmed' { return 'regressed, controls drifted' }
        'noise' { return 'regressed, unchanged library (noise)' }
        'improved' { return 'improved' }
        default { return 'within noise' }
    }
}

function Get-ControlText {
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Metric)
    if ($null -eq $Metric['ControlDriftPercent']) { return 'not observed' }
    $text = ([double]$Metric['ControlDriftPercent']).ToString('0.0', $script:Invariant) + '% max'
    if ($Metric['ControlDrifted']) { return "$text, beyond band" }
    return "$text, held"
}

function Get-MetricRow {
    param([Parameter(Mandatory)][string] $Scenario, [Parameter(Mandatory)][System.Collections.IDictionary] $Metric, [switch] $WithScenario)
    $cells = [Collections.Generic.List[string]]::new()
    if ($WithScenario) { $cells.Add($Scenario) }
    $cells.Add($Metric['Phase']); $cells.Add($Metric['Metric'])
    $cells.Add((Format-GateValue $Metric['Metric'] $Metric['BaselineMedian'])); $cells.Add((Format-GateValue $Metric['Metric'] $Metric['CandidateMedian']))
    $change = if ($null -ne $Metric['ChangePercent']) { Format-GatePercent $Metric['ChangePercent'] } elseif ([double]$Metric['BaselineMedian'] -eq [double]$Metric['CandidateMedian']) { '0.00%' } else { 'from zero' }
    $cells.Add($change); $cells.Add((Format-GateP $Metric['PValue']))
    $cells.Add($(if ($Metric['Exact']) { 'exact' } else { Format-GatePercent $Metric['NoisePercent'] '0.#' }))
    $cells.Add((Get-ControlText $Metric)); $cells.Add((Get-OutcomeText $Metric))
    return '| ' + (($cells | ForEach-Object { ConvertTo-MarkdownCell $_ }) -join ' | ') + ' |'
}

function ConvertTo-BenchmarkMarkdown {
    <# The job summary of one run: the conclusion and what it asks, what was compared, every scenario's outcome, the flagged
       metrics, and each scenario's full metric table (collapsed). #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Conclusion, [Parameter(Mandatory)][System.Collections.IDictionary] $Summary,
        [string] $Event = '', [string] $Pair = '', [switch] $Gate, [switch] $Hosted)
    $lines = [Collections.Generic.List[string]]::new()
    $name = $Conclusion['Conclusion']
    $lines.Add("## Paired benchmark: $(Get-BenchmarkHeadline $name)")
    $lines.Add('')
    $lines.Add((Get-BenchmarkMeaning $name))
    foreach ($note in $Conclusion['Notes']) { $lines.Add(''); $lines.Add($note) }
    $unresolved = @($Conclusion['Scenarios'] | Where-Object { $_['Held'] -lt $_['Judged'] })
    if ($name -ceq 'pass' -and $unresolved.Count) {
        # A pass is not a claim about the metrics the runner could not hold still for; say how many those are.
        $counts = @($Conclusion['Scenarios'] | ForEach-Object { "$(ConvertTo-MarkdownCell $_['Scenario']) $($_['Held']) of $($_['Judged'])" }) -join ', '
        $lines.Add('')
        $lines.Add("Resolution: the metrics that held inside their band in every same-binary control were $counts. A shift in the others that is smaller than the runner's own drift could not have been seen.")
    }
    if ($Conclusion['StrictControls']) { $lines.Add(''); $lines.Add('Strict controls are on: any unstable same-binary control makes its scenario inconclusive.') }
    if ($name -cne 'pass' -and -not $Gate) { $lines.Add(''); $lines.Add('This run was started by hand: the job stays green for a finding, and a pull request would fail on it.') }
    $lines.Add('')
    $baseline = $Summary['baseline']
    $candidate = $Summary['candidate']
    $short = { param([string] $Text) $Text.Substring(0, [Math]::Min(12, $Text.Length)) }
    # A side as its commit, with what it was asked as when that says more (a branch, a tag) and not when it is the commit or this checkout.
    $describe = {
        param($Side)
        if ($Side -isnot [System.Collections.IDictionary]) { return 'unknown' }
        $commit = [string]$Side['commit']
        $asked = [string]$Side['revision']
        $suffix = if ($Side['kind'] -ceq 'checkout') { ' (this checkout)' } elseif ($asked -and -not $commit.StartsWith($asked, [StringComparison]::OrdinalIgnoreCase)) { " ($asked)" } else { '' }
        return '`{0}`{1}' -f (& $short $commit), $suffix
    }
    $lines.Add('| | |')
    $lines.Add('|---|---|')
    $lines.Add("| Baseline | $(& $describe $baseline)$(if ($Pair) { " $Pair" }) |")
    $lines.Add("| Candidate | $(& $describe $candidate) |")
    $fingerprints = @($baseline, $candidate | ForEach-Object { if ($_ -is [System.Collections.IDictionary] -and $_['sourceFingerprint']) { "``$(& $short ([string]$_['sourceFingerprint']))``" } else { 'unknown' } })
    $lines.Add("| Library inputs | $(if ($Conclusion['LibraryUnchanged']) { "identical, fingerprint $($fingerprints[0]): both sides ran the same library code" } else { "changed, fingerprint $($fingerprints[0]) to $($fingerprints[1])" }) |")
    $lines.Add("| Method | $(if ($Summary['repetitions']) { "$($Summary['repetitions']) repetitions of the interleaved pass A, B, B, A, so $(2 * [int]$Summary['repetitions']) runs per side; " })$($Summary['platform']) $($Summary['configuration']); exact two-sided Mann-Whitney U test with the 5% timing and 2% memory investigation bands, exact budgets stay exact |")
    $lines.Add("| Runner | $(ConvertTo-MarkdownCell $Summary['machine'])$(if ($Hosted) { '; a shared hosted VM, not a controlled quiet desktop' }) |")
    if ($Event) { $lines.Add("| Event | $(ConvertTo-MarkdownCell $Event) |") }
    $lines.Add('')
    $lines.Add('### Scenarios')
    $lines.Add('')
    $lines.Add('| Scenario | Check | Set verdict | Regressed | Improved | Metrics held by controls | Controls stable |')
    $lines.Add('|---|---|---|---:|---:|---:|---:|')
    foreach ($scenario in $Conclusion['Scenarios']) {
        $controls = $scenario['Controls']
        $stable = $controls['Total'] - $controls['Unstable'] - $controls['Missing']
        $lines.Add("| $(ConvertTo-MarkdownCell $scenario['Scenario']) | $(Get-BenchmarkHeadline $scenario['Conclusion']) | $(ConvertTo-MarkdownCell $scenario['SetStatus']) | $($scenario['Regressed']) | $($scenario['Improved']) | $($scenario['Held']) of $($scenario['Judged']) | $stable of $($controls['Total']) |")
    }
    foreach ($scenario in $Conclusion['Scenarios']) {
        foreach ($note in $scenario['Notes']) { $lines.Add(''); $lines.Add("- **$(ConvertTo-MarkdownCell $scenario['Scenario'])**: $note") }
    }
    $flagged = @(foreach ($scenario in $Conclusion['Scenarios']) { foreach ($metric in $scenario['Metrics']) { if ($metric['Verdict'] -cin @('regressed', 'improved')) { [pscustomobject]@{ Scenario = $scenario['Scenario']; Metric = $metric } } } })
    $lines.Add('')
    $lines.Add('### Flagged metrics')
    $lines.Add('')
    if ($flagged.Count -eq 0) { $lines.Add('None: no metric regressed or improved.') }
    else {
        $lines.Add('| Scenario | Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |')
        $lines.Add('|---|---|---|---:|---:|---:|---:|---:|---|---|')
        foreach ($item in $flagged) { $lines.Add((Get-MetricRow -Scenario $item.Scenario -Metric $item.Metric -WithScenario)) }
    }
    foreach ($scenario in $Conclusion['Scenarios']) {
        if (@($scenario['Metrics']).Count -eq 0) { continue }
        $lines.Add('')
        $lines.Add("<details><summary>$(ConvertTo-MarkdownCell $scenario['Scenario']): all $(@($scenario['Metrics']).Count) metrics</summary>")
        $lines.Add('')
        $lines.Add('| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |')
        $lines.Add('|---|---|---|---:|---:|---:|---:|---|---|')
        foreach ($metric in $scenario['Metrics']) { $lines.Add((Get-MetricRow -Scenario $scenario['Scenario'] -Metric $metric)) }
        $lines.Add('')
        $lines.Add('</details>')
    }
    $lines.Add('')
    $lines.Add('The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. A metric regresses only when p < 0.05 and its median shift is beyond the band (a rise in an exact budget always regresses); `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.')
    return ($lines -join "`n") + "`n"
}

function Format-WorkflowCommand {
    <# A GitHub Actions workflow command (::error title=...::message) with the escaping the runner expects. #>
    param([Parameter(Mandatory)][ValidateSet('error', 'warning', 'notice')][string] $Level, [Parameter(Mandatory)][string] $Title, [Parameter(Mandatory)][string] $Message)
    $data = $Message.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    $property = $Title.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A').Replace(':', '%3A').Replace(',', '%2C')
    return "::$Level title=${property}::$data"
}

function Get-BenchmarkAnnotations {
    <# One annotation per scenario that is not a plain pass, at the level that matches what it means for the check. #>
    param([Parameter(Mandatory)][System.Collections.IDictionary] $Conclusion, [switch] $Gate)
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($note in $Conclusion['Notes']) { $lines.Add((Format-WorkflowCommand 'error' 'Paired benchmark: invalid evidence' $note)) }
    foreach ($scenario in $Conclusion['Scenarios']) {
        $flagged = @($scenario['Metrics'] | Where-Object { $_['Verdict'] -ceq 'regressed' } | ForEach-Object {
                '{0}/{1} {2} (p {3}, controls {4})' -f $_['Phase'], $_['Metric'], (Format-GatePercent $_['ChangePercent']), (Format-GateP $_['PValue']), $(if ($_['ControlDrifted']) { 'drifted' } else { 'held' }) })
        # An annotation stays short; the job summary lists every metric.
        $listed = (@($flagged | Select-Object -First 6) -join '; ') + $(if ($flagged.Count -gt 6) { "; and $($flagged.Count - 6) more (see the job summary)" } else { '' })
        switch ($scenario['Conclusion']) {
            'pass' { if ($flagged.Count) { $lines.Add((Format-WorkflowCommand 'notice' "Paired benchmark $($scenario['Scenario'])" "No regression established; flagged on unchanged library inputs (chance): $listed")) } }
            'degraded' { $lines.Add((Format-WorkflowCommand $(if ($Gate) { 'error' } else { 'warning' }) "Paired benchmark $($scenario['Scenario']): confirmed degradation" "$listed. The contract needs developer advice: optimize, reduce scope or defer.")) }
            'inconclusive' { $lines.Add((Format-WorkflowCommand $(if ($Gate) { 'error' } else { 'warning' }) "Paired benchmark $($scenario['Scenario']): inconclusive" "$(if ($flagged.Count) { "Flagged but not confirmed by same-binary controls: $listed. " })$($scenario['Notes'] -join ' ')")) }
            default { $lines.Add((Format-WorkflowCommand 'error' "Paired benchmark $($scenario['Scenario']): invalid evidence" ($scenario['Notes'] -join ' '))) }
        }
    }
    return $lines.ToArray()
}

function Add-WorkflowOutput {
    <# A step output (name=value on one line) in the file the runner reads, or on the console when there is none. #>
    param([Parameter(Mandatory)][string] $Name, [Parameter(Mandatory)][AllowEmptyString()][string] $Value)
    if ($Value -match '[\r\n]') { throw "A step output holds one line: $Name" }
    $line = "$Name=$Value"
    if ($env:GITHUB_OUTPUT) { [IO.File]::AppendAllText($env:GITHUB_OUTPUT, $line + "`n", [Text.UTF8Encoding]::new($false)) }
    else { Write-Host $line }
}

function Add-WorkflowSummary {
    <# Markdown for the job summary page, or on the console when there is no summary file. #>
    param([Parameter(Mandatory)][string] $Markdown)
    if ($env:GITHUB_STEP_SUMMARY) {
        [IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, $Markdown, [Text.UTF8Encoding]::new($false))
        # The step's log says how much the summary file holds now, so a run's log shows the page was written.
        Write-Host "Job summary: $((Get-Item -LiteralPath $env:GITHUB_STEP_SUMMARY).Length) bytes in the step summary file"
    }
    else { Write-Host $Markdown }
}

function Find-PairedSummary {
    <# The newest retained summary.json under <Root>/.build/paired/<run>/reports, or null. #>
    param([Parameter(Mandatory)][string] $Root)
    $paired = Join-Path $Root '.build/paired'
    if (-not (Test-Path -LiteralPath $paired -PathType Container)) { return $null }
    $found = @(Get-ChildItem -LiteralPath $paired -Directory | ForEach-Object { Join-Path $_.FullName 'reports/summary.json' } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Sort-Object { (Get-Item -LiteralPath $_).LastWriteTimeUtc } -Descending)
    if ($found.Count -eq 0) { return $null }
    return $found[0]
}

Export-ModuleMember -Function Get-BenchmarkGateScenarios, Get-BenchmarkGateRepetitions, Format-RepositoryPath, Get-BenchmarkScopeRules,
    Get-BenchmarkScope, Resolve-PullRequestPair, Get-PullRequestChangedPaths, Get-ControlDrift, Get-BenchmarkConclusion,
    Get-BenchmarkHeadline, ConvertTo-BenchmarkMarkdown, Format-WorkflowCommand, Get-BenchmarkAnnotations, Add-WorkflowOutput,
    Add-WorkflowSummary, Find-PairedSummary, Read-GateJson
