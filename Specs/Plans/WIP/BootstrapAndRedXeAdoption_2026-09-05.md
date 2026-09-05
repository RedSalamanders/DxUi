# Bootstrap and RedXe-first adoption

Status: ACTIVE
Date: 2026-09-05

The user approved the independent private DxUi project. Library contracts are indexed in
[Specs/README.md](../../README.md). RedXe owns its application/ABI plan; this plan owns library extraction.

## Execution

- [x] Create private `RedSalamanders/DxUI` and sibling checkout.
- [x] Bootstrap AGENTS, nine skills, normative contracts, validators, build/test scripts, source provenance and CI.
- [x] Extract the first independent Foundation static target from the existing frame runtime.
- [ ] Validate and publish bootstrap on all four native configurations; record local versus CI evidence.
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
Nine skill checks, specification/dependency validators, six tooling regression cases and formatting checks passed.
Native ARM64 execution and GitHub workflow results are recorded separately after publication. Renderer/input and AV
gates remain open; Foundation tests do not cover those capabilities.
