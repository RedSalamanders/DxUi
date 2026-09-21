# Native CI qualification of the grid candidate

Source: `7827873c1e211c62d17f65d4bf2e142e665085c3`, production V11 unchanged.
[Workflow 35642790795](https://github.com/RedSalamanders/DxUi/actions/runs/35642790795)
ran on native x64 Windows 2025 / VS2026 and ARM64 Windows 11 / VS2026 hosts.
This packet retains both attempts, suite/build logs, executable hashes, linked
complex-UI measurements, workflow status and downloaded artifact identities.
`raw/sha256.receipt.txt` fingerprints every copied input. Original JSON files use
the `.json.receipt.txt` suffix to distinguish native evidence from independent
performance-comparison inputs; their contents are unchanged.

| Native profile | Final regular suites | Explicit skipped cases | External consumer |
| --- | --- | --- | --- |
| x64 Debug, attempt 2 | 18 passed | 0 | passed |
| x64 Release | 18 passed | 0 | passed |
| x64 ASan Debug | 18 passed | 0 | both annotation modes passed |
| ARM64 Debug | 18 passed | 9, Menu only | passed |
| ARM64 Release | 18 passed | 9, Menu only | passed |
| ARM64 ASan Debug | 18 passed | 9, Menu only | both annotation modes passed |

Both native AddressSanitizer probes intentionally exit 1 and report `detected:true`;
this is successful detection, not a regular-suite pass or an unexpected failure.
Grid, Embedded, Rendering and Accessibility pass without skips in all six final
profiles. The ARM64 Menu skips explicitly require an interactive desktop; these
jobs do not qualify those physical menu-input journeys.

## Retained initial failure

The first x64 Debug Accessibility attempt fails
`TestDisclosureNotifiesNativeAutomationClient` during its three-second setup
deadline: stage `ElementFromHandle`, elapsed 3047 ms, readiness false. The printed
`E_PENDING` is the still-initialized diagnostic field while the worker is inside
the call, not evidence that ElementFromHandle returned that HRESULT.

After the workflow completed, one classification retry of job 106475898821 ran
as job 106486198229 against the same source. Setup completed in 78 ms and the
full x64 Debug job passed. No timeout, test or production code was changed.
This is an intermittent setup failure, not a claimed code repair or established
environmental cause. `native-x64-Debug` preserves the failure;
`native-x64-Debug-attempt2` contains the successful retry.

## Limits and remaining gates

These are native functional/platform receipts. CI's complex-UI values are
unpaired and cannot close the retained original/candidate resource regression.
The grid memory tradeoff has not been approved. Consumer pin adoption, native
product DPI/assistive-technology acceptance and the separately owned menu work
remain outside this packet. No consumer checkout or dependency changed.

This archive changes only evidence and documentation. Production, test fixtures
and gallery inputs remain those of the tested source; no new pixel generation
is required for the archival change.
