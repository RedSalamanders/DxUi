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
- [ ] D1: decouple retained controls and tests; remove concrete WindowHost and application-helper dependencies.
- [ ] D1: deliver Controls, Win32Services and Win32Host static targets plus a standalone WindowHost sample.
- [ ] D2: implement embedded host, coherent preparation/composition, dirty surfaces and resource pool.
- [ ] D2: standalone EmbeddedHost sample, WARP, capture, text/IME, UIA and resource benchmarks.
- [ ] D3: RedXe pins the tested revision; implement generic preparation, input, text/UIA bridges and integration fixture.
- [ ] D3: rebuild every source-coordinated plugin and update RedXe normative contracts with the supported ABI.
- [ ] D4: AV adopts shared controls and responsive layouts; its audio/camera backend gates remain in RedXe's AV plan.
- [ ] Close library/bootstrap scope only after native validation and normative closeout. Move this plan to Done.

## Import transition

Use the exact source inventory and dependency findings in [provenance](../../../provenance/source-import.json).
The original reference remains unchanged. Extract reviewed slices into active source and record each in
[the ledger](../../../provenance/migration-ledger.md). Do not claim that the Foundation build exercises imported controls.
RedSalamander remains on its in-tree implementation during this plan.

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
