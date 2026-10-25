# DxUi local qualification at 2fed8246

This packet records the clean source commit `2fed82465dcee9f213753754e0e8e54f353cf38f`, tree `2c0641bc363bf713cfc709d721c3f6549dd5aef8`. The later merge commit `97b26bad7fcdb0aa3dbb2efd4bceac77aab5a295` has the same tree; source identity and merge identity remain separately recorded.

## Six-profile local matrix

All six x64 and ARM64 Release, Debug, and ASan Debug builds passed. Each x64 profile passed 19 ordinary suites with zero skips; all 57 suite receipts bind to the retained profile executables. The ARM64 profiles are cross-builds only and do not claim native runtime coverage. The x64 ASan profile also detected the intentional use-after-free canary, separate from its 19 passing suites.

The 74 build artifacts and 317 payload streams are represented by ordinal and SHA-256 in `summary.md`; private custody retains raw receipts and payloads. The public-header comparison covers 25 headers with identical Git blobs, not the whole source tree.

The separate [lead byte replay](lead-replay.md) rehashes those 74 artifacts and 317 payload streams without retagging the original execution.

## Foreground qualification

The separate [foreground packet](../sdk-2fed8246-foreground/README.md) binds four authorized x64 Release interactive suites to the retained Release executable. All four passed with zero skips, and the lease recorded unchanged foreground, focus, and cursor state.

## Remaining gates

Hosted native results are recorded in separate packets. The current PR paired benchmark remains pending, and the earlier CD35 paired result is inconclusive under an unqualified base policy. No performance acceptance, quiet-host qualification, downstream RedTilde acceptance, physical IME, or assistive-technology result is claimed.
