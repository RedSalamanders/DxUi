# Indexed Unicode source qualification

The bounded rich-text model now finds pieces by binary-searching cumulative
source ends. The end replaces the existing length field; there is no additional
descriptor field, prefix array, mutable read cache, worker or timer. Slice and
equal-range checks start at the first relevant piece. Edits still copy
descriptors, and projection/layout still prepare the complete bounded document.
The 64-Ki UTF-16 / 128-history limits and public API revision 4 are unchanged.
This closes one D2a substep, not the complete engine or large-document gate.

Two new independent oracles cover 4,096-fragment Unicode/CRLF lookup and slices,
coalescing, snapshot/history retirement, and concurrent immutable reads while
the owner edits and resets its document. The same test-source bytes ran before
and after implementation. Five fixed 8,192-query Release trials report:

| Observation | Before | Indexed |
| --- | ---: | ---: |
| Median lookup trial | 6.5310 ms | 0.7234 ms |
| Query checksum | 5,431 | 5,431 |
| Document / snapshot object bytes | 88 / 16 | 88 / 16 |

These are one fresh process per revision, with five within-process trials. They
are descriptive, not a paired regression verdict, convergence proof or quiet-host
claim. Every trial is retained. The unchanged canonical fixture remains
`E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E`.
All four post-change canonical reports are unpaired; no historical baseline,
band, judge, policy or warning is replaced. Performance acceptance remains HOLD.

The qualified library source fingerprint is
`4EDDEF355659895319D81501089066D93E8F6885C2701A0C19C149C17C2EC014`.
The pre-change library is `E3437165...` from the preceding UIA implementation;
before-source/artifact hashes and the exact oracle source hash are retained.
Collection used the feature checkout at HEAD 42ec1984 with recorded dirty code,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges`, and MSBuild node reuse disabled.

Release EditorControls/Embedded/Accessibility passed first. The fixed subsequent
matrix ran the remaining 16 Release suites, all 19 Debug suites and all 19 ASan
Debug suites, then ARM64 Debug/Release/ASan Debug cross-builds. All **57 ordinary
x64 suite runs** have zero capability skips, including 43 named rich-text
scenarios per profile. All six builds pass. The driver
completed in 1,752.662 seconds within its 3,600-second budget, with 900 seconds per
owned child. ARM64 was not executed. The isolated sanitizer probe specifically
diagnosed heap-use-after-free. Native/embedded rich-text, source text services,
UIA and WARP scenarios run in the current suites; Embedded retains 5,155 checks
and 1,000 warm composites without C++ allocations in each profile.

Six additional serial current-build diagnostics exercise timers Off/On in
Release, Debug and ASan Debug. All four clear/composition/readback controls,
ordered QPC/schema checks, zero clean C++ allocation/preparation, surface reuse
and hidden release/no-work checks pass. The six processes completed in 158.364
seconds within 900 seconds overall and 120 seconds per native child. Their
1,920 probe frames are functional/resource evidence, separate from the earlier
[baseline/candidate investigation](../2026-10-18-uia-boundaries/README.md).
All six captures match that study's populated scene byte for byte.

All five validators, the complete tooling suite (125 profiling cases included)
and clang-format 22.1.3 pass. Detailed original build logs include both earlier
diagnostic setup attempts and the indexed-source matrix. The initial filtered
baseline launch was refused because linking had not finished; it ran no native
suite and remains retained alongside the later completed baseline. No failed
run, outlier or comparison was replaced.

## Retention and verification

Native contains all 57 exact suite/log pairs and the real sanitizer probe.
Performance contains their four original linked reports/comparisons. Build holds
all six actual successful attestation receipts and eleven original detailed build
logs. Raw holds pre-change results, matrix/validation/format logs, declared
budgets and completion inventories. Diagnostics retains all six stamped reports,
exact native bytes, owned-child records, logs and captures. Raw/metadata JSON
payloads use `.receipt.txt` aliases without changing their bytes. The before/after
lookup observations are also retained in lookup-analysis.receipt.txt.

Run `Verify.ps1 -SourceRoot <checkout-with-the-recorded-profiling-helper-bytes>`.
It checks SHA256SUMS when present, raw-byte bindings, native/stamped equality,
57 suite/performance/build links, the specific sanitizer diagnosis and all six
current-profile diagnostics. It does not launch tests or manufacture acceptance.
The frozen helper closure is retained in the preceding boundary packet for replay
after later tooling changes. Old packets remain untouched.

Capabilities remain the bounded v0 preview. Gallery pixels/content are unchanged
by this nonvisual lookup and diagnostic work. Consumer pins are untouched.
Full parser/extensions, incremental layout/shared preparation, semantic rich UIA,
actual IME/AT, agreed-time foreground checks, native ARM64 and independently
trusted performance qualification remain open in the active plan.

