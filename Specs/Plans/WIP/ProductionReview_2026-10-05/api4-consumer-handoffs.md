# API revision 4 consumer handoff register

Status: **OPEN**. This record tracks work owned by consumers after the DxUi API revision 4 candidate is independently
reviewed and an exact library commit is available. It does not update consumer pins, publish a library revision, or
qualify any consumer product.

The candidate's public compile/import contract is frozen in
[`Tools/ConsumerApi/Revision4.cpp`](../../../../Tools/ConsumerApi/Revision4.cpp) and is compiled by the exact-pin
consumer gate. Passing that fixture establishes the library's declared revision-4 source contract only. Each consumer
must adopt its own exact pin and run its product tests against the published candidate. The current library candidate
is still in review; this register intentionally names no candidate commit.

| Consumer | Consumer-owned handoff | Evidence still required | Status |
| --- | --- | --- | --- |
| RedXe | Update its exact DxUi pin to the reviewed revision-4 commit; keep the single `DxUi.lib` integration; forward the actual `PointerEvent::device` from embedded input where touch feedback is intended. | Consumer restore/build and relevant product suites; real touch, IME, screen-reader/UIA and AV-device acceptance remain RedXe-owned release gates. | OPEN; no consumer edit or pin update is part of this library change. |
| RedSalamander | Update the consumer's exact pin from the previously recorded adoption revision (`13788e95`) to the reviewed revision-4 commit; forward pointer-device provenance from its embedded adapter where applicable. | Fresh product Debug/Release adoption evidence, ARM64/ASan qualification, public exact-pin restore, library publication and consumer merge/release. The prior local x64 adoption record applies only to its recorded pin. | OPEN; see [`RedSalamanderMigration.md`](../../Done/RedSalamanderMigration.md). |
| RedPrism | Verify the consumer's current pin before adoption; the Tree plan records historical pin `271bd54` as predating #35. Adopt the reviewed revision-4 library and continue using the shared Tree for the layer panel. | Product-specific Tree multi-select/reorder behavior and the consumer's own build/test matrix; publish and merge only through its owner workflow. | OPEN; no pin or product change is made here. |

## Handoff completion record

For each consumer, its owner should record the exact DxUi commit and API revision, pin/lock change, restore and build
configuration, relevant product test results and remaining capability skips. A pin is not qualified by this register or
by the library's frozen fixture. Device, platform, interactive and publication results must link to consumer-owned
receipts or release records; an absent result remains **NOT RUN**. Recheck historical pin values in the target consumer
checkout before acting, because this library record is not a live inventory of consumer branches.
