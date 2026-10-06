# Embedded interaction layout — 2026-10-06

## Delivery checklist

- [x] Preserve the consumer's failed six-case rapid layer interaction run and preceding 10,000-row cost observations.
- [x] Implement explicit caller-arranged interaction acknowledgement without drawing or surface allocation.
- [x] Pass the x64 Debug test gate (matrix-r22): all 20 requested suites exit 0.
- [x] Pass the x64 Release test gate (matrix-r23): all 20 requested suites exit 0.
- [x] Pass the x64 ASan Debug test gate (matrix-r24): all 20 requested suites exit 0; the isolated negative ASan probe detects the expected heap-use-after-free. Menu desktop-dependent cases listed below remain skipped, not covered.
- [x] Pass ARM64 Debug, Release and ASan Debug cross-build gates (matrix-r25–r27); these are cross-compilation results, not ARM64 runtime qualification.
- [x] Exercise the interaction-layout test in each x64 profile: 158 checks; the complete Embedded suite reports 2,992 checks.
- [x] Qualify literal state transitions, capture safety, wrong-thread calls, failed preparation and hidden/device/extent cases in the 158 independent checks, preserving existing assertions.
- [x] Pass full `validate.ps1` and `format.ps1 -Check` after the final test fixture repair; validation/format logs `interaction-layout-validate-r2.log` and `interaction-layout-format-r2.log` are preserved under `.build`.
- [x] Review paired Release common-scene measurements against the retained API-3 baseline and retain every noisy round/control; six runs per side are within the existing noise/resource budget.
- [ ] Qualify the RedPrism pin, unchanged interactions, no-paint publication invariant and fresh largest-row owner measurements.
- [ ] Publish normative/usage documentation and complete the library delivery; consumer G1 remains separately owned.

Status: **ACTIVE.** The six library build/test gates and paired measurement passed for the recorded source. RedPrism pin qualification, application G1 and consumer closeout remain open.

## Prepared-tree matrix receipts

The results below were independently read from each prepared-tree `result.json`, child log, suite reports and archived
readback manifest. Every result has `validationPassed: true`, exit code 0, 201 frozen source files and 10 products;
the archive readback verifies every listed file. Diagnostic counts are 43 for each x64 Debug/Release receipt, 45 for
x64 ASan Debug, and one build log for each ARM64 receipt. The identical-source count is the frozen input count; it does
not establish paired performance. Primary comparison verifies every frozen copy and current input byte and finds
identical `{path, bytes, SHA256}` entries across all six configurations. SHA256 of ordinal-sorted UTF-8 rows
`path<TAB>bytes<TAB>uppercase-hash<LF>` is `D25A5D7933DF3509E8B65647D2D7A94956793CF89CAAE046E7A03B0AAC2DCD47`.

| Gate | Prepared result | Archived receipt | Readback entries (all verified) | Archive readback-manifest SHA-256 |
| --- | --- | --- | ---: | --- |
| x64 Debug Test | `.build/prepared-tree-proof/matrix-r22/x64/Debug/Test` | `D:\DxUiQualificationArchives\P1-interaction-layout-Debug22-2026-10-06` | 302 | `82585C16CF9F1BDC0D01F3F52A68A157C9ED69D704885756C04750417AB1530E` |
| x64 Release Test | `.build/prepared-tree-proof/matrix-r23/x64/Release/Test` | `D:\DxUiQualificationArchives\P1-interaction-layout-Release23-2026-10-06` | 302 | `E120EA04C2CE47A9DDF8205FCEAD7A42703553CD23C3371234EC735925F39F30` |
| x64 ASan Debug Test | `.build/prepared-tree-proof/matrix-r24/x64/ASan Debug/Test` | `D:\DxUiQualificationArchives\P1-interaction-layout-ASan24-2026-10-06` | 306 | `1D38F1D64AC594F9D8E35073543E915D229E95169ACB047AD5571F3C1A2D7EE1` |
| ARM64 Debug Build | `.build/prepared-tree-proof/matrix-r25/ARM64/Debug/Build` | `D:\DxUiQualificationArchives\P1-interaction-layout-ARM64-Debug25-2026-10-06` | 219 | `152F9C480022B644B957A44C8FBBB565FD807C747F5367915B4D3466030718DA` |
| ARM64 Release Build | `.build/prepared-tree-proof/matrix-r26/ARM64/Release/Build` | `D:\DxUiQualificationArchives\P1-interaction-layout-ARM64-Release26-2026-10-06` | 219 | `5BE218E4A833F081B28064C8F6B169EC815E5D62A8EC0F326CD3D4FE30667ADF` |
| ARM64 ASan Debug Build | `.build/prepared-tree-proof/matrix-r27/ARM64/ASan Debug/Build` | `D:\DxUiQualificationArchives\P1-interaction-layout-ARM64-ASan27-2026-10-06` | 219 | `4FCF6B1ED32D7645FA8854E1D578986A77AD7F20A59B2FE32FB59D6DD917DEDF` |

For each x64 profile, `test-Embedded` reports `TestInteractionLayoutWithoutPaint` with 158 checks and 2,992 total Embedded
checks. In the ASan receipt, `diagnostics/reports/AddressSanitizer-x64.json` records `detected: true`, `exitCode: 1`
for the isolated negative probe; `diagnostics/logs/test-AddressSanitizer-x64.log` contains the corresponding
heap-use-after-free report, while the enclosing test run exits 0. The probe is expected evidence of sanitizer
detection, not a product-suite failure.

The ASan Menu report records 18 capability skips because an interactive desktop is unavailable. The exact skipped
tests are:

- `TestWindowHostTabRaisesAutomationFocusChanges`
- `TestWindowHostTreeArrowAnnouncesTheFocusedItem`
- `TestWindowHostElementSetFocusAnnouncesOnlyThatElement`
- `TestWindowHostClickThatActivatesAWindowUiAutomationHasAskedBeforeAnnouncesTheClickedControlOnce`
- `TestWindowHostClickThatActivatesAWindowUiAutomationHasNotSeenLeavesTheClickedControlToItsFirstGetFocus`
- `TestWindowHostReactivatingAWindowUiAutomationHasSeenAnnouncesItsFocusedControlOnce`
- `TestWindowHostThatLostTheForegroundAnnouncesNoFocusChange`
- `TestDescribedSubmenuSliderFocusKeepsSessionFocus`
- `TestMenuItemRoleOutsideMenuPopupTransfersNativeFocus`
- `TestContextMenuDebugStateProbeBoundsWedgedWindowThread`
- `TestSplitButtonContextMenuSentMouseMessagesHoverAndInvokeImmediately`
- `TestSplitButtonContextMenuOwnerMessageFloodDoesNotStarvePointerInput`
- `TestMenuKeyboardTabExitsMenuLoop`
- `TestMenuKeyboardF10ExitsMenuLoop`
- `TestMenuKeyboardAltExitsMenuLoop`
- `TestNativeMenuBarRestoresFocusAfterMenuDismiss`
- `TestNativeMenuBarNestedPopupCanDestroyHostSafely`
- `TestSplitButtonContextMenuSentMouseMessagesHoverAndOutsideDismiss`

NewControls repeats the first seven UIA focus cases as capability skips. These skips are not passing coverage. The
ASan evidence is under `.build/prepared-tree-proof/matrix-r24/x64/ASan Debug/Test/diagnostics`; the original child,
diagnostics, frozen inputs and products are retained in the corresponding `D:\DxUiQualificationArchives` receipt.

## Failure and scope

RedPrism's state-only SyncChrome repair reduces 10,000-layer owner request/apply costs from over 22 ms to below
0.4 ms, but Debug r234 rejects six rapid layer Tree interaction expectations. Changing header visibility advances
DxUi's interaction revision. Waiting for frame painting leaves intervening gestures incoherent. Original failed
receipts remain immutable in the consumer's test-run record; expectations are unchanged.

PrepareInteraction is optional and additive at API revision 3. Existing consumers keep their Prepare boundary.
The host first arranges all hit geometry itself, then acknowledges only the last successful painted extent/DPI.
No worker, swap chain, periodic wakeup or graphics ownership changes. Full painting still owns text geometry,
surface allocation and accessibility publication. Failure, capture and unavailable transitions are prescribed by
the [embedded contract](../../Rendering/Rendering_EmbeddedD3D11.md).

No visual style changes are intended. Existing gallery pictures remain representative; usage docs must explain
the new optional input boundary. A screenshot cannot prove ordered input, cancellation or resource behavior.

## Qualification

The first full Debug matrix-r21 runs 19 suites successfully and fails the new Embedded destroyed-capture setup:
the fixture clicked x=203 without setting its splitter to x=200. The primary adds the declared position/minima;
the capture expectation stays unchanged. This is a fixture repair, not a library pass. Actual child 108636 exits 1
from 10:00:36.5816523 to 10:04:32.6820111 UTC; all 201 inputs, ten products and diagnostics are preserved and read
back in `D:\DxUiQualificationArchives\P1-interaction-layout-Debug21-fixture-failed-2026-10-06`. Its 302-copy manifest
SHA256 is `9CC051124B7A23D2102FE26FE7DB7777A2A56EC2F878F570C17FC89621527134`.

Use library-owned scenes and literal coordinates/model values as the state-machine reference. Keep every existing
embedded assertion. Test initial/full failure, rapid visibility/enabled/bounds transitions without paint, a canceled
slider draft, retained sibling-only drag, destroyed capture, hide/zero/device/size/DPI and cross-thread refusal.
Compare surface/preparation/composition counters before and after interaction acknowledgement. Frame preparation
must remain pending, and text/UIA geometry must remain unavailable until painting catches up. The RedPrism consumer
then replays its original failed interaction fixture at the exact qualified pin and all existing configuration gates.

ARM64 builds are cross compilation only on this host. Library scene timings do not prove application G1.

## Paired Release measurement

The quiet interleaved A/B/B/A comparison finishes at 10:55:01 UTC, with three passes (six runs per side),
baseline `271bd54be24eadf3f03ba5221d1be7be069860f2` and the candidate working source on `8fbaed5`.
Neither side needs a harness overlay. Candidate library fingerprint is
`D7406CC62C0C08CE8E5E35CFDE4800E1E3DABF60B96D1243007ABDC638CD6E2D`;
all twelve receipts use benchmark fingerprint `F9D66318E82EB344391494C5302785443D254A8AE18AB65C5FBC9FE80E60A5AE`.
All six candidate executables match the matrix-r23 tested product SHA256
`57D658C6401A2FF8237C8CE48D4C2267D4EF0087140F00381EEF30B3361E8AF2`.

The Default library-owned scene is `within-noise-budget`, with zero regressed or improved metrics. Clean frame
p95 median is 0.46835 / 0.45555 ms (baseline/candidate); dirty is 1.74265 / 1.75 ms. Both use 3,686,400-byte
surface and replacement peaks, zero clean/compose allocations, and 2,160 dirty C++ allocations. Same-source
drift observations remain in the raw comparisons. This verdict establishes no regression in this scene;
it is not proof of equality, a consumer latency gate or a visible frame measurement.

Original receipts and the actual helper remain under
`D:\DxUiQualificationArchives\P1-interaction-layout-paired-2026-10-06-r1\20261006T105450Z-271bd54be24e\reports`;
reviewed library-only copies are retained in [Measurements/EmbeddedInteractionLayout](../../../Measurements/EmbeddedInteractionLayout/README.md).

Closeout validation r3 rejects the aggregate `summary.json` as a standalone fixture receipt. The retained copy is
named `paired-summary.comparison.json` without changing its bytes; the original remains in the qualification
directory. Specification validation r4 and full validation r5 then pass. No validator or threshold is relaxed.
