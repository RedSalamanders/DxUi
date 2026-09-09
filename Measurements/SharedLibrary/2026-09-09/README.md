# Shared-library I19 qualification, 2026-09-09

The [current native CI receipt](../../../Specs/Plans/WIP/SharedLibraryReadiness/native-ci-ecad707.json)
qualifies ecad707 in all six native profiles: [run 34371134464](https://github.com/RedSalamanders/DxUi/actions/runs/34371134464).
It adds repeated EXE/two-DLL animation and native-menu ownership probes to the
complete suites, public consumer builds and galleries; both ASAN annotation policies pass.
Local x64 Debug, Release and ASan Debug full suites also pass.

The [previous native CI receipt](../../../Specs/Plans/WIP/SharedLibraryReadiness/native-ci-52da33d.json)
records all six passing profiles at `52da33d8152a737d698d5362308cd16a34491f42`:
[run 34363702073](https://github.com/RedSalamanders/DxUi/actions/runs/34363702073).
Every profile ran the complete control/embedded suites, external pinned consumer
fixture and gallery. Both ASAN architectures passed the deliberate defect probe
and both consumer STL annotation policies. Native ARM64 execution is established.
This includes the public native provider, four restored callback-lifetime cases and
three interactive menu fixtures with aligned cursor/capture setup. The
[earlier receipt](../../../Specs/Plans/WIP/SharedLibraryReadiness/native-ci.json)
at ebc3796 is preserved; these test results do not substitute for paired performance.

## Paired resource evidence remains open

Retained here are all three baseline/candidate pairs for each x64 Debug/Release
profile, including failed comparison budgets. Baseline is d192e47; the candidate
production source is 8c548fe (ebc3796 changes only capture-fixture settling and docs).
The source/fixture/compiler/output identities in each raw receipt govern the result.
Other product tests were active on the local host; these are not quiet-run acceptance.

Release pair 1 exceeded timing and memory budgets; pair 2 exceeded memory budgets;
pair 3 passed. Debug pairs exceeded different timing/memory budgets. The inconsistent
direction does not establish a confirmed regression or acceptance. Preserve every
result, characterize repeated-baseline variance and repeat paired measurements under
controlled load. All pairs retained zero composition allocations, surface growth and
hidden work. Do not select only the passing pair or silently relax/rebaseline budgets.

G4 clipboard receipts and prior baseline samples remain under Measurements/Clipboard.
Consumer runtime, packaging, resource and hardware qualification remain consumer-owned.

## Repeated baseline and candidate at ecad707

[I19-ecad707-quiet](I19-ecad707-quiet/) retains fourteen further runs:
one reference baseline and three baseline/candidate pairs per Debug/Release
profile. There was no local compiler/linker activity during this series.
All six repeated-baseline comparisons exceeded investigation bands. Debug dirty
composition rose 7.9–11.0% in the three paired comparisons; Release paired outcomes
were mixed. Neither acceptance nor a confirmed causal regression follows.

[I19-ecad707-cpu-subset](I19-ecad707-cpu-subset/) retains all 32 follow-up runs,
16 paired comparisons and 14 repeated-baseline comparisons. Both binaries inherited
the same eight-logical-processor affinity (mask 255), with pair order alternating
baseline/candidate then candidate/baseline. The process affinity was restored;
no machine-wide power/priority setting changed. The context receipt records this
additional fixture condition. Original executable/workload hashes and comparison
thresholds are unchanged; these results are compared within this series only.

Five of seven A/A comparisons still exceed bands in each profile. Across the eight
per-run medians, Debug dirty composition changes +0.61% and Release +0.00%; the
prior Debug composition signal does not repeat at its earlier magnitude. Aggregate
FPS changes range from -1.23% to +0.34%, private bytes from -0.87% to +1.07%, and
working set from -0.14% to +1.51%. These descriptive medians do not replace the
individual paired verdicts: eleven of sixteen pairs still trigger investigation.
All raw samples and failed comparisons remain visible. Composition allocation,
surface and hidden-work checks show no growth. Acceptance remains open pending a
stable fixture; thresholds and the original baseline have not been relaxed.
