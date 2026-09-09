# Shared-library I19 qualification, 2026-09-09

The [native CI receipt](../../../Specs/Plans/WIP/SharedLibraryReadiness/native-ci.json) records all six passing profiles at
`ebc37965d624febd42f41ef777b0e5363a7a3a8b`:
[run 34351569531](https://github.com/RedSalamanders/DxUi/actions/runs/34351569531).
Every profile ran the complete control/embedded suites, external pinned consumer
fixture and gallery. Both ASAN architectures passed the deliberate defect probe
and both consumer STL annotation policies. Native ARM64 execution is established.

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
