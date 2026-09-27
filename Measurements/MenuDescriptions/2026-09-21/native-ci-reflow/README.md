# Native scrollbar-reflow qualification

Source `7ada987f3d211b2b6522ca18a5692d22512fbd98`.
[CI 35650904698](https://github.com/RedSalamanders/DxUi/actions/runs/35650904698)
completed successfully in all six native profiles. Existing text-layout pairs are
reflowed for the scrollbar instead of replacing a second complete set. French
DPI 192â†’96â†’144â†’96 coverage checks complete text bounds and no cumulative narrowing.

| Native profile | Passing suites | Explicit skips |
| --- | ---: | --- |
| ARM64-ASan Debug | 18 | Menu: 9 |
| ARM64-Debug | 18 | Menu: 9 |
| ARM64-Release | 18 | Menu: 9 |
| x64-ASan Debug | 18 | 0 |
| x64-Debug | 18 | 0 |
| x64-Release | 19 | 0 |

The extra x64 Release suite is the v2 resource-scaling diagnostic. Nine pre-existing
ARM64 Menu desktop-capability skips remain unqualified; zero skips elsewhere.
All six external consumers and both ASan annotations-disabled variants pass.
Expected failing ASan probes report detection. These are library results, not
consumer mixed-monitor or assistive-technology acceptance.

All six x64 Release gallery PNGs are byte-identical to diagnostic baseline
`356006a`. No changed production pixels or API are intended. Raw suite receipts
preserve executable hashes and linked common-UI measurements. Their JSON bytes
are stored with `.receipt.txt` suffixes; the SHA256 inventory binds all copied
inputs. Full artifacts remain in `C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-7ada987-final`.
CI performance is unpaired and does not accept any memory tradeoff.

The subsequent local v3 measurement fixture positions its owned window on one
native monitor and records every popup extent. It retains a local
positioning failure: the actual menu can remain at its temporary 1×1 HWND even
though its internal bounds are 476×322. Anchoring alone did not fix that failure.
No local resource comparison or product repair is claimed yet.

**Archive pruning (2026-09-25):** 151 verbose logs and raw receipt copies (1.05 MB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
