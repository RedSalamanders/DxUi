# Bootstrap and RedXe-first adoption

Status: ACTIVE
Date: 2026-09-05

The user approved the independent private DxUi project. Library contracts are indexed in
[Specs/README.md](../../README.md). RedXe owns its application/ABI plan; this plan owns library extraction.

## Execution

- [x] Create private `RedSalamanders/DxUI` and sibling checkout.
- [x] Bootstrap AGENTS, nine skills, normative contracts, validators, build/test scripts, source provenance and CI.
- [x] Extract the first independent Foundation static target from the existing frame runtime.
- [x] Build/test Foundation on all four native configurations, locally for x64 and in GitHub for ARM64; publish on main.
- [x] Complete hosted bootstrap validation after the import-manifest correction.
- [x] Make DxUI the canonical source home; move controls/tests, remove duplicate runtime and application projects.
- [x] Validate owned-source layout, pending dependency inventory, formatting and local Foundation regressions.
- [x] D1: decouple retained controls and tests; public ControlHost replaces application-specific host naming.
- [x] D1: user superseded split targets with one DxUi.lib containing controls and both hosting modes; native gallery fixture retained.
- [x] D2: implement embedded host, coherent preparation/composition, dirty surfaces and resource pool.
- [ ] D2/D3: finish RedXe-specific embedded text/IME/UIA bridges and manual touch/accessibility acceptance. Supplied-device sample and WARP/resource tests are delivered by the single-library plan.
- [ ] D3: RedXe pins the tested revision; implement generic preparation, input, text/UIA bridges and integration fixture.
- [ ] D3: rebuild every source-coordinated plugin and update RedXe normative contracts with the supported ABI.
- [ ] D4: AV adopts shared controls and responsive layouts; its audio/camera backend gates remain in RedXe's AV plan.
- [ ] Close library/bootstrap scope only after native validation and normative closeout. Move this plan to Done.

## Ownership and dependency transition

DxUI is the canonical owner of `src/Controls` and `Tests/Controls`; shared fixes happen in place here.
Historical attribution is recorded in [source origin](../../../provenance/source-origin.json), with original bytes
recoverable from Git history. The duplicate source tree and obsolete application projects have been removed.
The frame runtime is consolidated into Foundation and controls use the neutral DxUi namespace.
Remove [pending dependencies](../../../provenance/pending-dependencies.json) as standalone targets are enabled;
record progress in [the ledger](../../../provenance/migration-ledger.md).
Foundation tests do not exercise pending controls. RedSalamander remains on its old implementation during this plan.

## Validation evidence

Local verification on 2026-09-05: all four Foundation builds passed; x64 Debug/Release tests passed.
Nine skill checks, specification/dependency validators and formatting checks passed locally. A consumer fixture
built and executed through the pinned props/targets from a path with spaces, using isolated dependency outputs;
an incorrect revision was rejected. Tooling regression cases cover import corruption and developer-local exclusions.

[GitHub run 33958428386](https://github.com/RedSalamanders/DxUI/actions/runs/33958428386) passed all four native
Foundation test jobs, including ARM64 Debug/Release. Its Linux validation identified an ignored `.user` file in the
import inventory. That developer-local file was never committed; it is removed from the snapshot and manifest and
now explicitly rejected by the validator. [Corrective run 33958577659](https://github.com/RedSalamanders/DxUI/actions/runs/33958577659)
passed all five jobs at commit `316e39cbdfc20eea619020b5d4e31e4f395b34e5`: Linux validation and all four native test configurations.
Renderer/input and AV gates remain open; Foundation tests do not cover those capabilities.

Ownership correction validation on 2026-09-05: all owned C++ source/tests pass formatting, all nine skills and
30 Markdown documents validate, and 13 tooling regressions pass. Historical origin records cover 56 files with
54 current ownership paths and two retired project files; 21 source files have explicit pending include records.
Local x64 Debug/Release Foundation tests and ARM64 Debug/Release builds passed after the move. Controls remain
pending runtime validation; the layout change does not close their D1/D2 acceptance gates.

Current library delivery is tracked by [SingleLibraryPublicControls](SingleLibraryPublicControls_2026-09-05.md); the earlier Foundation-only statements above are historical evidence.
