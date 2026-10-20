# Native Unicode image rendering

The retained RichTextEditor now displays caller-supplied immutable native pixels.
Local PNG/JPEG/BMP/GIF/TIFF decoding is an explicit caller-worker utility; the
control owns no worker, HWND, device, filesystem watcher or periodic timer.
Clipboard image paths become displayed Markdown image syntax. Explicit document
location supplies the relative base; unsaved/cross-root paths use file URIs.

## Qualification

All six canonical builds pass: x64 Debug 5:18, Release 5:48, ASan Debug 5:44;
ARM64 Debug 5:21, Release 6:32, ASan Debug 4:40. ARM64 is cross-build evidence,
not native runtime execution. All 19 ordinary suites pass on each x64 profile
(57 scope runs, zero capability skips). The deliberate isolated ASan use-after-free
probe is detected, establishing active instrumentation. Final before/after v2
input/artifact checks pass for each runtime profile; cross-build attestations
are separately verified. Compiler and test helpers use the same recorded
process-local workspace TEMP/TMP and node-reuse environment.

The [qualification identities](qualification.json), [run output](runs.receipt.txt),
[directed driver](directed-harness.ps1.receipt.txt) and
[ordinary driver](ordinary-harness.ps1.receipt.txt) bind the tested source,
profiles and artifacts. Complete raw packets, copied binaries and original
failures remain under `.build/reports/rich-image-native-m5g2-20261020`.

Coverage includes independent known pixels, PNG premultiplied alpha, lossless
BMP/GIF/TIFF, a JPEG color oracle, all eight real TIFF orientation cases,
partial GIF canvas/background/transparency, Unicode real-file I/O, writer sharing,
read failure/EOF/cancellation/capacity and immutable strong output. Shared native/
embedded cases cover coalescing, stale source/base/mode/visibility identities,
source/selection/history preservation, missing alternatives, opaque-image
selection tint, RTL/DPI, hiding and explicit device replacement without repeat I/O.
Native/embedded UIA checks cover Unicode names, actual bitmap bounds, exact raw
RangeFromChild and old-fragment retirement at an unchanged source revision.

The actual application adapter test performs clipboard-to-Markdown-to-MTA-file
decode-to-owner-message-to-WARP-pixels, with one source undo. A separate three-batch
96-result lifecycle test covers duplicate submission, 32-result/32-pending queue
saturation, strong overflow, default UNC refusal, cancellation pruning/resumption
and closure. The Debug schedules sweep 70 path/URI/clipboard allocations, 12
immutable decode/create allocations, 142 resource-state allocations and 217 loaded
Unicode geometry allocations: 441 observed points, with exact retry. Raster
checks inject first/later memory/device HRESULT failures, verify one upload per
owner turn and share one bitmap over 128 admitted placements while retaining the
129th alternative. These assertions also run in ordinary Release/ASan Embedded.

Preserved negatives include oversized SDK line-cell bounds, Unicode inline-run
splitting, duplicate-image event double counting, canceled-result queue wake-up,
C1128 object sections, the optimized correlation-test reader warning, stale
MSBuild failure markers, system-temp exhaustion and an exact-environment
attestation refusal before execution. The fixes preserve authoritative source,
public display maps and alternatives. No warning, test or attestation guard is
suppressed.

The [guide and actual light/dark captures](../../../docs/rich-text.md) show the
sample with explicit synthetic caller pixels and a missing-image alternative;
canonical capture bytes match the reviewed published PNGs. File decoding is
independently covered by the real asynchronous worker test. General themed
control gallery regeneration/review is recorded with this change.

All benchmark timings emitted by ordinary tests are unpaired diagnostics.
**D4c remains HOLD.** Process/GPU/transient resource qualification, physical
IME/AT/agreed foreground checks, native ARM64 runtime, other M5/M6 milestones
and consumer adoption remain open; this functional packet does not close them.
