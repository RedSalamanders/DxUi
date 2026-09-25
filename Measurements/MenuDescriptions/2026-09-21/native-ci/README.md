# Native qualification before scrollbar allocation optimization

Source `c1f047b1567607eb331852255e1ed563855e638d`.
[CI 35646866096](https://github.com/RedSalamanders/DxUi/actions/runs/35646866096)
completed successfully in all six native x64/ARM64 Debug, Release and ASan Debug
profiles. Each profile passed eighteen regular suites. x64 Release additionally
passed the separately retained [v1 menu resource probe](../scaling-v1/README.md).

x64 suites have zero skips. ARM64 Menu has nine explicit pre-existing desktop
capability skips per profile; all other ARM64 suites have zero skips. Those menu
pointer/focus/destruction journeys remain unqualified on the ARM64 CI desktop.
The new described async-menu Edit-child focus restoration journey reports no
capability skip. These receipts do not replace native product mixed-DPI or
assistive-technology acceptance.

All six external consumers pass; both ASan profiles also pass the annotations-
disabled consumer. Intentional ASan probes exit1 with `detected:true` as expected.
The earlier `a474cf2` focus assertion failure remains recorded in the WIP plan;
this source corrects the assertion and contract to preserve the pre-existing
async-root/modal-owner focus distinction and tests owner-child restoration.

Raw suite reports retain executable hashes, architecture and linked common-UI
measurements. Focused Menu/NewControls/Accessibility/WindowHost logs and build
logs accompany them. JSON receipts use `.receipt.txt` suffixes without changing
their bytes; `raw/sha256.receipt.txt` fingerprints copied evidence. The original
artifact archives remain under `C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-c1f047b`.

These are functional qualification results for the named source. CI performance
is unpaired and does not accept the outstanding memory tradeoff. The subsequent
heap diagnostic and scrollbar allocation optimization require their own
qualification; no consumer pin has changed.

**Archive pruning (2026-09-25):** 165 verbose logs and raw receipt copies (1.13 MB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
