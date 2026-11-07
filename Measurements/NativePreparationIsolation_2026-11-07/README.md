# Native preparation isolation and qualification fixtures

Implementation `2c8b267e2a26c914faa873e3557d4d2c6691de66` isolates expected,
checked native content-preparation failures. A failed editor does not withhold a
healthy sibling's frame or input. The failed control stays dormant across unrelated
intrinsic continuations; explicit invalidation or resource retirement can rearm it.
Cooperative `S_FALSE` work continues normally. A reentrant newer preparation request
survives an older failing result. Unavailable editor layouts retain source, selection
and history without accepting stale-layout gestures.

The final whole-source [qualification ledger](qualification.json) records the
source, binaries, build diagnostics, suite receipts and independently checked original
archive members. ARM64 local entries are cross-builds. This scope changes no gallery
pixels or application dependency pin.

The [counterexamples](counterexamples.json) preserve a same-object native fixture
against retained baseline `93052665`, an intermediate implementation and the final
preparation implementation. The baseline withholds the healthy sibling's real WARP
pixels; the intermediate retries an unchanged failed control during unrelated root
continuations; the candidate passes. The maintained fixture independently checks
pixels only in the healthy label's rectangle, sibling input, explicit recovery,
cooperative continuation and ordinary edit/undo/redo behavior.

Two qualification fixtures are corrected without lowering their assertions. Fatal
scenarios 13–16 distinguish the live filter-capture stack from the original exception
context: the record requires the stable noinline `NativeInvokeTrampoline`, while the
saved-context dump must independently reach `DxUiModuleEscapingNativeOrigin` with
the exact image, symbols, thread and instruction pointer. Diagnostic x64 replay over
four original ARM64 records accepts that capture witness and rejects the old throw
witness and wrong module/name controls. It is not native ARM64 execution or a
saved-context/PAC qualification. The actual Menu destruction test reuses the existing
foreground-held sequence; it retries only observed foreground loss and preserves
activation, popup, destruction and post-loop assertions. No local foreground run is
claimed for this candidate.

Failed tooling fixtures now retain their owned original directory and report its
path. Successful fixtures still clean up. The same maintained child test fails with
the old helper and passes with the new helper, independently checking the original
five-byte payload and the child's failing exit code before deliberate cleanup.

The [first-attempt record](first-attempts.json) keeps separate the earlier x64-only
`c3a401c8` cohort, the first combined source's missing diagnostic-label compile error,
the mixed-source diagnostic-only static run, interrupted Debug builds, and original
hosted failures. Later green checks do not retrospectively pass those attempts or
assign a cause to older missing-log failures.

The durable contracts are in [native hosting](../../Specs/Rendering/Rendering_Win32Host.md),
[controls and layout](../../Specs/UI/UI_ControlsAndLayout.md),
[failure handling](../../Specs/Core/Core_FailureHandling.md),
[validation](../../Specs/Testing/Testing_Validation.md) and
[run evidence](../../Specs/Testing/Testing_RunEvidence.md). Current API-5 hosted native
ARM64, real Menu/consumer execution and the combined test-before-production gate
remain required. Parser/preparer admission across individual diagrams, math and
images, physical input/AT and independent performance acceptance remain owned by
the [parent plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
