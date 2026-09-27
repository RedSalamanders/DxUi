# Ordinary ComplexUi menu header source ablation, 27 September 2026

This packet retains a balanced six-invocation x64 Release WARP ablation of the clean-frame-P95 signal. The execution order was A1, H1, B1, B2, H2, A2. All six ordinary `performance.ps1 -SkipBuild` invocations exited successfully. A reused its exact verified V1 build; V1's later H dependency-preflight refusal is retained and V1 ran no benchmark. H dependencies were restored and H/B were freshly built. The prior V1 failure is setup evidence, not a benchmark failure or result.

A is `fb857d44f093da3ad62a2cb0cf090e2b5bc13bef`; H starts at the same commit with only the two candidate debug-state float fields added to `include/DxUi/DxUi.h`; B is `36a1c437fa404c3296aafb6c2d3ac20e8c4566e5`, changing `src/Controls/DxUi.Menu.cpp` relative to H. The three 63-file library source inventories confirm only those sequential deltas, and the A/H/B executable `.text` hashes are all distinct. Eight benchmark/tool inputs match byte-for-byte across variants.

| Comparison | Pair | Clean frame P95 | Comparator flags | Status |
|---|---|---:|---:|---|
| first-A1-H1 | A1 → H1 | +7.442% | 5 | advice-required |
| first-H1-B1 | H1 → B1 | -3.643% | 1 | advice-required |
| first-A1-B1 | A1 → B1 | +3.528% | 4 | advice-required |
| second-A2-H2 | A2 → H2 | +9.230% | 8 | advice-required |
| second-H2-B2 | H2 → B2 | +1.595% | 3 | advice-required |
| second-A2-B2 | A2 → B2 | +10.972% | 8 | advice-required |
| control-A1-A2 | A1 → A2 | -3.345% | 2 | advice-required |
| control-H1-H2 | H1 → H2 | -1.737% | 3 | advice-required |
| control-B1-B2 | B1 → B2 | +3.605% | 3 | advice-required |

The A→H clean frame-P95 increase repeats at +7.442% and +9.230%. H→B changes are −3.643% and +1.595%; A→B is +3.528% and +10.972%. The latter crosses the existing 5% timing threshold only in the second pair. Every saved comparator result is `advice-required`; this reflects one or more metric flags in each comparison, not failed benchmark executions and not a waiver.

Same-variant controls also carry flags: A1→A2 has dirty prepare P95 +5.161% and dirty compose CPU P95 +16.848%; H1→H2 has clean private bytes +2.833%, private peak +2.804%, and dirty compose CPU P95 +55.349%; B1→B2 has dirty frame P95 +5.977%, prepare P95 +6.948%, and compose CPU P95 +12.500%. These controls show timing/resource movement within variants. They do not cancel the repeated A→H clean frame-P95 flag.

The H executable was built from a C: managed worktree while A/B used Z: worktrees. Despite the distinct `.text` hashes and controlled source deltas, this C:/Z: image/path difference remains a confound. The ablation localizes where the saved measurements move but does not establish that the header change caused the repeated flag. It does not establish non-regression or change the baseline. Lightweight review/file work and independent applications were not excluded; this is not an idle-only hard-budget result. The fixture measures completed offscreen WARP frames with blocking one-pixel readback; it does not measure swap-chain presentation or vsync. The unchanged comparator thresholds remain authoritative.

`raw/reports/*.json` are the six standard benchmark reports. Other payloads use short content-addressed `.receipt.txt` names to bound Windows checkout paths; identical payloads are stored once and derived patches are explicitly labelled `@derived`; `raw/original-file-map.json.receipt.txt` maps each retained source path to its staged path, SHA256 and byte size. V1 setup-failure records, the dependency-restore log, six invocation receipts, all nine comparator outputs, logs, inputs, manifests, build records, source snapshots/deltas and the original V2 file-hash inventory are retained. `SHA256SUMS` binds every staged payload except the manifest itself.

After reviewing the reported grid and menu costs, the user explicitly answered **"Accept the recorded timing tradeoffs"** on September 27. The owning performance contract records that limited acceptance separately from these unchanged raw comparator failures. Correctness, combined-build/resource, DPI and accessibility qualification remain required. This does not accept unbounded future regression or establish a new baseline.
