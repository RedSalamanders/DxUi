# Quote inner margins and recoverable sequence-label allocation

Status: DONE — scoped implementation, x64 native regressions, exact-pin consumer checks and domain documentation

## Delivered behavior

- [x] Reserve six DIP inside both horizontal sides of quote/callout surfaces during semantic geometry preparation.
- [x] Apply the same content measure to nested containers, lists, table allocation and placement, source ACP hit testing and both reading directions; clamp narrow padding to retain text width.
- [x] Keep native layout reuse and draw/resource rules; do not create a separate paint-time text offset.
- [x] Reproduce the old geometry failure and add the registered `TestRichSemanticQuoteInsets` regression.
- [x] Preserve the first Debug Embedded abort, symbolize its exact executable/PDB and repair both fallible sequence-label staging paths.
- [x] Add a numbered sequence/note case to the existing individually injected semantic/geometry allocation sweep; verify retry and source/publication invariants.
- [x] Update UI/resource contracts, rich-text usage, the component guide/gallery, native captures and change fragment.
- [x] Verify RedTilde's explicit API-4 adoption at `8c6c2818d084e6124fb2c08eddc00bdba7a9d2aa` with native document-scrollbar coverage.

Durable requirements live in [controls/layout](../../UI/UI_ControlsAndLayout.md) and
[performance/resources](../../Core/Core_PerformanceAndResources.md), with public usage in
[rich-text documentation](../../../docs/rich-text.md).

## Native evidence and failure diagnosis

Implementation `8c6c2818d084e6124fb2c08eddc00bdba7a9d2aa` passes the complete EditorControls, Embedded and
Accessibility suites in x64 Debug, ASan Debug and Release with zero capability skips. Each build reports
zero warnings/errors. The broader x64 Release run `20261104T094137414Z-50320-44ccd8cc790d4b8d816f7d22f131b845`
passes all 19 noninteractive native scopes with zero capability skips. The real SDK ASan detection probe passes. Six native gallery sheets are regenerated
and reviewed; their capture metadata retains the original working-tree provenance.

| Profile | Retained run |
| --- | --- |
| x64 Debug | `20261104T090417734Z-52528-315e16989d9247ae848d45931bfe6ad3` |
| x64 ASan Debug | `20261104T091640098Z-52528-6ff9aca1111f4e6b92275a016b13ef0f` |
| x64 Release | `20261104T093438630Z-52528-86f7f2aa65f94345baba7b5ba523716d` |

The first failed Debug Embedded executable at the preceding quote checkpoint has SHA-256
`02A7A4DD3FF236F8670E4707560F5E8C0F9C9E2B2FC045A22C32B9C06309FDF5`.
Its exact program/symbols, raw run `20261104T083021476Z-56648-5c64b6fa21474feeb0249b6a6bf44d74` and bounded diagnostic stacks
are retained privately. MSVC Debug container-proxy allocation occurred inside noexcept empty-string/format
staging. The unchanged pre-existing path was exposed by required qualification. Explicit fallible staging
and construction preserve the existing checked preparation failure/retry behavior. A subsequent numbered
sequence counterexample and the final passing injected-allocation diagnostic remain separately retained.

RedTilde independently passes all 74 native cases in all three x64 configurations, real ASan detection and
34 Release captures at the exact pin. Its
[consumer packet](https://github.com/RedSalamanders/RedTilde/blob/6c7f63615368f710399d64dcf10ec7a0e3a3f5a3/Measurements/QuoteMarginsAndScrollbars_2026-11-04/README.md)
retains source, binary, capture and scenario identities. Library publication never advances another consumer's lock.

## Remaining acceptance

The [single active plan](../WIP/RemainingWork_2026-11-04.md) retains the broader lifetime/editor/parser work,
API-5 candidate, native ARM64 and physical input/AT qualification, performance policy and consumer acceptance.
Cross-builds, unpaired diagnostic timings and source publication do not close those gates.
