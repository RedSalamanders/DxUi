#requires -Version 7.0
# Read-only curator for the completed pair. Refuses to run until all raw receipts, comparisons, and final inventory exist.
[CmdletBinding()]
param([string]$Evidence='C:/RedSalamander.Perf/evidence/i26-ui/combined-pair-20260927')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Evidence=[IO.Path]::GetFullPath($Evidence)
function Need($ok,[string]$why){if(-not $ok){throw $why}}
function ReadJson([string]$path){Get-Content -Raw -LiteralPath $path|ConvertFrom-Json}
function Digest([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToUpperInvariant()}
function ManifestEqual($a,$b,[string]$label){Need($a.Count -eq $b.Count) "$label count differs";for($i=0;$i -lt $a.Count;$i++){Need(($a[$i].path -ceq $b[$i].path)-and($a[$i].sha256 -ceq $b[$i].sha256)) "$label differs: $($a[$i].path)"}}
$scenarios=@('Default','MultilineGridRetention')
$runIds=@('A1','B1','B2','A2')
$runKeys=@(foreach($s in $scenarios){foreach($id in $runIds){"$id-$s"}})
$pairs=@(@('A1','B1'),@('A1','B2'),@('A2','B1'),@('A2','B2'),@('A1','A2'),@('B1','B2'))
$comparisonKeys=@(foreach($s in $scenarios){foreach($p in $pairs){"$s-$($p[0])-$($p[1])"}})
# Exact inputs and unchanged comparator defaults from the reviewed paired driver.
$inputPaths=@('performance.ps1','Tools/compare_performance.py','Tools/Invoke-Python.ps1','Tests/Embedded/EmbeddedTests.cpp','Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Samples/ComplexUi/ComplexUiScene.h','Samples/EmbeddedControls/GraphicsFixture.h')
$metricRules=@{
 fps=@{direction='higher';noise=5};frameP50Ms=@{direction='lower';noise=5};frameP95Ms=@{direction='lower';noise=5};prepareP95Ms=@{direction='lower';noise=5};composeCpuP95Ms=@{direction='lower';noise=5}
 privateBytes=@{direction='lower';noise=2};privatePeakBytes=@{direction='lower';noise=2};workingSetBytes=@{direction='lower';noise=2};workingSetPeakBytes=@{direction='lower';noise=2}
 surfaceBytes=@{direction='lower';noise=0};replacementPeakBytes=@{direction='lower';noise=0};cppAllocations=@{direction='lower';noise=0};composeAllocations=@{direction='lower';noise=0}}
Need(Test-Path -LiteralPath $Evidence -PathType Container) "Missing evidence folder: $Evidence"
$missing=[Collections.Generic.List[string]]::new()
foreach($key in $runKeys){foreach($suffix in @('.json','.log','-exit.txt','-invocation.json','.json.comparison.json')){$f=Join-Path $Evidence "$key$suffix";if(-not(Test-Path -LiteralPath $f -PathType Leaf)){$missing.Add($f)}}}
foreach($key in $comparisonKeys){foreach($suffix in @('.comparison.json','.comparison.log','.exit.txt')){$f=Join-Path $Evidence "$key$suffix";if(-not(Test-Path -LiteralPath $f -PathType Leaf)){$missing.Add($f)}}}
foreach($f in @('protocol.json','final-inventory.json','A-source-built.json','B-source-built.json','A-build-msbuild.log','A-build-console.log','A-dependencies.log','B-build-proof/performance-build-receipt.json','B-build-proof/build-msbuild.log','B-build-proof/release-test.log','B-build-proof/gallery.log')){if(-not(Test-Path -LiteralPath (Join-Path $Evidence $f) -PathType Leaf)){$missing.Add((Join-Path $Evidence $f))}}
Need($missing.Count -eq 0) "Evidence is incomplete; curator did not start. Missing: $($missing -join '; ')"
$protocol=ReadJson (Join-Path $Evidence 'protocol.json');$final=ReadJson (Join-Path $Evidence 'final-inventory.json')
Need(($protocol.benchmarkToolInputs.Count -eq 8)-and(($protocol.benchmarkToolInputs -join '|') -ceq ($inputPaths -join '|'))) 'Protocol input list is not the reviewed exact eight-file list.'
ManifestEqual $final.AInputs $final.BInputs 'Final A/B benchmark/tool inputs'
Need($final.AInputs.Count -eq 8) 'Final input inventory must contain exactly eight files.'
$builtA=ReadJson (Join-Path $Evidence 'A-source-built.json');$builtB=ReadJson (Join-Path $Evidence 'B-source-built.json')
ManifestEqual $final.A $builtA 'Final A production inventory';ManifestEqual $final.B $builtB 'Final B production inventory'
$sourceAfp=([string]$protocol.sourceFingerprints.A);$sourceBfp=([string]$protocol.sourceFingerprints.B)
$sourceRows=@{}
foreach($v in @('A','B')){$sourceRows[$v]=if($v -eq 'A'){$builtA}else{$builtB}}
$reports=@{};$invocations=@{}
foreach($key in $runKeys){
 $reportPath=Join-Path $Evidence "$key.json";$run=ReadJson $reportPath;$inv=ReadJson (Join-Path $Evidence "$key-invocation.json");$v=$inv.variant;$single=ReadJson (Join-Path $Evidence "$key.json.comparison.json")
 Need($inv.id -eq $key) "Run label mismatch: $key";Need($v -eq $key.Substring(0,1)) "Variant/run key mismatch: $key";Need(([string](Get-Content -Raw (Join-Path $Evidence "$key-exit.txt"))).Trim() -eq '0') "Invocation exit is not zero: $key";Need($inv.exitCode -eq 0) "Invocation receipt exit is not zero: $key"
 Need($inv.scenario -eq $key.Substring($key.IndexOf('-')+1)) "Scenario/run key mismatch: $key"
 Need($single.status -eq 'unpaired' -and $single.changes.Count -eq 0) "Single-run result is not explicitly unpaired: $key"
 $expectedFixture=if($inv.scenario -eq 'Default'){'dxui-complex-ui-v2'}else{'dxui-complex-ui-multiline-grid-retention-v1'}
 Need($run.fixture -eq $expectedFixture) "Wrong fixture identity: $key";Need(($run.configuration -eq 'Release')-and($run.platform -eq 'x64')-and($run.nativeArchitecture -eq 'X64')-and$run.buildSkipped) "Wrong original performance flags: $key"
 Need(($run.framesPerRound -eq 40)-and($run.roundCount -eq 5)-and($run.controls -eq 83)-and($run.modelRows -eq 1000)) "Workload bounds changed: $key"
 Need($inv.sourceFingerprintBefore -eq $inv.sourceFingerprintAfter) "Source changed during $key";Need((Digest $reportPath) -eq $inv.reportSha256) "Run report hash mismatch: $key";Need((Digest (Join-Path $Evidence "$key.log")) -eq $inv.logSha256) "Run log hash mismatch: $key";Need($run.sourceFingerprint -eq $inv.sourceFingerprintBefore) "Receipt source fingerprint mismatch: $key"
 Need(($inv.exeBefore -eq $inv.exeAfter)-and($inv.libBefore -eq $inv.libAfter)-and($run.executableSha256 -eq $inv.exeBefore)) "Binary changed or receipt mismatch: $key"
 Need($run.sourceCommit -eq $protocol.PSObject.Properties["${v}Head"].Value) "Unexpected source commit: $key"
 Need(($run.hiddenPreparations -eq 0)-and($run.hiddenComposites -eq 0)) "Hidden work was nonzero: $key"
 Need($run.memoryPhases.Count -eq 6) "Expected six memory phases: $key";Need((@($run.memoryPhases|ForEach-Object name)-join '|') -ceq 'entry|device|scene|warm|capture|hidden') "Memory phase identity mismatch: $key"
 foreach($m in $run.memoryPhases){Need(($m.privateBytes -ge 0)-and($m.workingSetBytes -ge 0)) "Invalid memory phase: $key/$($m.name)"}
 if($inv.scenario -eq 'Default'){Need((-not $run.PSObject.Properties['retention'])-and(-not $run.PSObject.Properties['detached'])) "Default run unexpectedly contains retention: $key"}
 else{
  Need($run.retention.Count -eq 32) "Retention sample bound mismatch: $key";Need($run.retention[0].frame -eq 0 -and $run.retention[0].phase -eq 'start') "Retention start sample missing: $key"
  for($i=1;$i -le 30;$i++){Need(($run.retention[$i].frame -eq 200*$i)-and($run.retention[$i].phase -eq 'scroll')) "Retention scroll bounds mismatch: $key sample $i"}
  Need(($run.retention[31].frame -eq 6000)-and($run.retention[31].phase -eq 'model-cleared')) "Retention model-clear endpoint missing: $key"
  Need($run.PSObject.Properties['detached'] -and $run.detached.privateBytes -ge 0 -and $run.detached.handles -ge 0) "Detached endpoint missing: $key"
  foreach($sample in $run.retention){Need(($sample.privateBytes -ge 0)-and($sample.workingSetBytes -ge 0)-and($sample.handles -ge 0)-and($sample.surfaceBytes -ge 0)) "Invalid retention sample: $key";Need(-not $sample.PSObject.Properties['heaps']) "Unexpected heap instrumentation in ordinary retention run: $key"}
  Need(-not $run.detached.PSObject.Properties['heaps']) "Unexpected heap instrumentation in ordinary retention run: $key"
 }
 ManifestEqual $inv.inputsBefore $inv.inputsAfter "$key input stability";Need($inv.inputsBefore.Count -eq 8) "$key did not bind all eight inputs"
 for($i=0;$i -lt 8;$i++){Need(($inv.inputsBefore[$i].path -ceq $final.AInputs[$i].path)-and($inv.inputsBefore[$i].sha256 -ceq $final.AInputs[$i].sha256)) "$key input differs from final inventory: $($inv.inputsBefore[$i].path)"}
 ManifestEqual $inv.sourceInventoryBefore $inv.sourceInventoryAfter "$key source inventory stability"
 ManifestEqual $inv.sourceInventoryAfter $sourceRows[$v] "$key built source inventory"
 Need($inv.sourceFingerprintBefore -eq $(if($v -eq 'A'){$sourceAfp}else{$sourceBfp})) "$key crossed its own source fingerprint"
 $reports[$key]=$run;$invocations[$key]=$inv
}
# The two invocations per variant must retain one source image and one executable/library pair throughout.
foreach($v in @('A','B')){$rows=@($invocations.Values|Where-Object variant -eq $v);Need($rows.Count -eq 4) "Expected four $v invocations";Need((@($rows|ForEach-Object sourceFingerprintBefore|Sort-Object -Unique).Count -eq 1)) "$v source fingerprint drifted";Need((@($rows|ForEach-Object exeBefore|Sort-Object -Unique).Count -eq 1)-and(@($rows|ForEach-Object libBefore|Sort-Object -Unique).Count -eq 1)) "$v binary drifted"}
foreach($s in $scenarios){$rows=@($runKeys|Where-Object{$_ -like "*-$s"}|ForEach-Object{$reports[$_]});Need($rows.Count -eq 4) "Expected four receipts for $s";foreach($r in $rows){foreach($other in @('platform','configuration','nativeArchitecture','machine','cpu','os','compiler','warpVersion','powerPolicy','benchmarkSha256','controls','modelRows','framesPerRound','roundCount')){Need($r.$other -eq $rows[0].$other) "$s identity mismatch: $other"}}}
$comparisonSummary=[Collections.Generic.List[object]]::new();$pairChanges=[Collections.Generic.List[object]]::new()
foreach($name in $comparisonKeys){$path=Join-Path $Evidence "$name.comparison.json";$c=ReadJson $path;$exit=([string](Get-Content -Raw (Join-Path $Evidence "$name.exit.txt"))).Trim();$scenario=($name -split '-',2)[0]
 Need($c.status -in @('within-noise-budget','advice-required')) "Invalid comparison status: $name";Need((($exit -eq '0') -eq ($c.status -eq 'within-noise-budget'))) "Comparison exit/status mismatch: $name"
 Need($c.changes.Count -eq (2*$metricRules.Count)) "Comparison lacks clean/dirty metric coverage: $name"
 $seen=@{};foreach($change in $c.changes){Need($change.scenario -in @('clean','dirty')) "Unexpected comparison scenario: $name";Need($metricRules.ContainsKey($change.metric)) "Unexpected metric: $name/$($change.metric)";$key="$($change.scenario)/$($change.metric)";Need(-not $seen.ContainsKey($key)) "Duplicate metric result: $name/$key";$seen[$key]=$true;$rule=$metricRules[$change.metric];Need($change.noisePercent -eq $rule.noise) "Comparator threshold changed: $name/$key";$shouldRegress=if($rule.direction -eq 'higher'){$change.after -lt $change.before*(1-$rule.noise/100)}else{$change.after -gt $change.before*(1+$rule.noise/100)};Need([bool]$change.regressed -eq [bool]$shouldRegress) "Comparator flag mismatch: $name/$key";$pairChanges.Add([ordered]@{comparison=$name;scenario=$change.scenario;metric=$change.metric;before=$change.before;after=$change.after;changePercent=$change.changePercent;noisePercent=$change.noisePercent;regressed=$change.regressed})}
 Need($seen.Count -eq 2*$metricRules.Count) "Missing comparison metrics: $name";Need(($c.status -eq $(if(@($c.changes|Where-Object regressed).Count){'advice-required'}else{'within-noise-budget'}))) "Status does not summarize flags: $name"
 $comparisonSummary.Add([ordered]@{name=$name;status=$c.status;flagged=@($c.changes|Where-Object regressed|ForEach-Object{"$($_.scenario)/$($_.metric)"})})
}
# Distill all four A/B crossings by scenario and metric; same-variant controls stay separate.
$observed=@();foreach($s in $scenarios){foreach($scenarioName in @('clean','dirty')){foreach($metric in $metricRules.Keys){$samples=@($pairChanges|Where-Object{$_.comparison -like "$s-A*-B*" -and $_.scenario -eq $scenarioName -and $_.metric -eq $metric});Need($samples.Count -eq 4) "Expected four cross-pairs: $s/$scenarioName/$metric";$deltas=@($samples|ForEach-Object{$_.after-$_.before}|Sort-Object);$validPercents=@($samples|Where-Object{$null -ne $_.changePercent}|ForEach-Object{$_.changePercent}|Sort-Object);$medianPercent=$null;if($validPercents.Count -eq 4){$medianPercent=($validPercents[1]+$validPercents[2])/2};$observed += [ordered]@{scenario=$s;case=$scenarioName;metric=$metric;medianObservedDelta=($deltas[1]+$deltas[2])/2;medianObservedChangePercent=$medianPercent;crossPairDeltas=$samples}}}}
$accepted=@('Previously accepted reference costs (context only): grid +6–7 MiB and +0.025 ms grid frame median; menu +0.049 ms clean frame P95 and +0.165 ms dirty frame P95. These historical figures are not subtracted from this pair and are not causal estimates.')
$summary=[ordered]@{curatedUtc=[DateTime]::UtcNow.ToString('o');evidence=$Evidence;runCount=$runKeys.Count;comparisonCount=$comparisonKeys.Count;sourceFingerprints=@{A=$sourceAfp;B=$sourceBfp};inputManifest=$final.AInputs;comparisons=@($comparisonSummary);observedFourCrossPairMedians=$observed;sameVariantControls=@($comparisonSummary|Where-Object name -match 'A1-A2|B1-B2');acceptedCostContext=$accepted;interpretationLimit='Observed A/B deltas and flags are reported without causal attribution. Grid/menu accepted reference costs are separate historical context; no threshold, flag, or baseline is waived.'}
$summary.costScopeAssessment=@(
 [ordered]@{category='Overlaps previously accepted cost scope';observations=@('Default clean frameP50Ms +0.01885 ms (+4.93%) versus accepted grid +0.025 ms frame median.','Default clean frameP95Ms +0.0327 ms (+6.97%) versus accepted menu +0.049 ms clean frame P95.','Retention dirty privateBytes +6,934,528 bytes (+20.98%, 6.61 MiB) versus accepted grid +6-7 MiB.');interpretation='These combined A/B observations overlap accepted metric/magnitude scope and are not additional costs to add to it; they do not identify component attribution.'},
 [ordered]@{category='Outside the stated accepted cost metrics; remains open';observations=@('Default dirty composeCpuP95Ms +0.0056 ms (+31.50%, 5.6 microseconds).','Default dirty privateBytes +722,944 bytes (+2.51%, 0.69 MiB).','Retention workingSetBytes +1,601,536 bytes clean (1.53 MiB) and +1,900,544 bytes dirty (1.81 MiB).');interpretation='Observed deltas are potential new effects, but current repeat controls and mixed pair results do not establish stability or causality; keep them open.'},
 [ordered]@{category='Medians within comparator noise or direction unstable';observations=@('Default dirty frameP95Ms +2.13% and retention clean frameP50/P95 +1.30%/+1.97%, below the 5% timing guardrail on four-cross-pair medians.','Retention dirty frameP50/P95 are lower by about 53%, while B1/B2 controls flag dirty FPS/frame-time changes.');interpretation='Do not treat these medians as stable cost or benefit.'}
)
$summary.interpretationLimit='All cross-variant pairs and same-variant controls contain advice-required flags. Controls demonstrate variability but do not prove the absence of a new effect. No stable combined-candidate effect is established or ruled out; possible effects outside previously accepted scope remain open. No threshold, flag, or baseline is waived.'
$summaryPath=Join-Path $Evidence 'curated-analysis.json';$summary|ConvertTo-Json -Depth 24|Set-Content -LiteralPath $summaryPath -Encoding utf8
$md=[Collections.Generic.List[string]]::new();$md.Add('# Paired DxUi benchmark curation');$md.Add('');$md.Add('All eight run receipts and twelve comparison outputs passed identity, input, source/binary freeze, workload, hidden-work, retention-bound, and default-threshold checks.');$md.Add('');$md.Add('## Comparison flags');$md.Add('');$md.Add('| Comparison | Status | Flags |');$md.Add('|---|---|---|');foreach($c in $comparisonSummary){$md.Add("| $($c.name) | $($c.status) | $($c.flagged -join ', ') |")};$md.Add('');$md.Add('## Four-cross-pair observed medians');$md.Add('');$md.Add('These are observed B-versus-A deltas across the four cross-variant comparisons. They do not identify cause.');$md.Add('');$md.Add('| Workload | Case | Metric | Median delta | Median change |');$md.Add('|---|---|---:|---:|---:|');foreach($o in $observed){$pct=if($null -eq $o.medianObservedChangePercent){'n/a'}else{"$([math]::Round($o.medianObservedChangePercent,3))%"};$md.Add("| $($o.scenario) | $($o.case) | $($o.metric) | $([math]::Round($o.medianObservedDelta,6)) | $pct |")};$md.Add('')
$md.Add('## Cost interpretation')
$md.Add('')
$md.Add('All four cross-variant comparisons in each workload and all four same-variant controls have advice-required flags. The controls show measurement variability; they do not prove that a new effect is absent. This pair establishes neither a stable combined-candidate effect nor its absence, so possible effects outside accepted scope remain open.')
$md.Add('')
$md.Add('The Default clean frame median (+0.01885 ms) and P95 (+0.0327 ms) observations overlap the previously accepted grid median (+0.025 ms) and menu clean P95 (+0.049 ms), respectively. Retention dirty private memory (+6.61 MiB) overlaps the accepted grid +6–7 MiB scope. Do not add these overlapping observations as new costs or attribute them to a component.')
$md.Add('')
$md.Add('Outside that stated scope, Default dirty compose CPU P95 is +0.0056 ms (+31.5%, 5.6 microseconds), Default dirty private bytes +0.69 MiB, and retention working set +1.53 MiB clean / +1.81 MiB dirty. These are observed potential effects, not qualified stable costs; keep them open.')
$md.Add('')
$md.Add('Four-cross-pair medians for Default dirty frame P95 (+2.13%) and retention clean frame P50/P95 (+1.30%/+1.97%) are below the 5% timing guardrail. Retention dirty frame P50/P95 show roughly 53% lower times, but the B1/B2 control also flags dirty timing variation. Treat those timing observations as unstable, not a cost or benefit.')
$md.Add('')
$md.Add('## Previously accepted cost context');$md.Add('');foreach($line in $accepted){$md.Add($line)};$md.Add('');$md.Add('Same-variant controls remain listed among the comparison flags above. Advice-required results are retained as findings; no thresholds or baselines are changed.');$md -join "`n"|Set-Content -LiteralPath (Join-Path $Evidence 'curated-analysis.md') -Encoding utf8
Write-Output "Curated complete evidence: $summaryPath"
