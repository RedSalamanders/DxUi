# Grid UI Automation GridPattern

## Status

Completed 2026-10-08. The implementation passed all 19 requested noninteractive suites in x64 Debug, Release and ASan
Debug, with 10 existing NewControls capability skips in each profile, and the same three native profiles on ARM64.
The identified x64 RedSalamander consumer candidate passed two selected native cases: the analysis-host
detached-provider/callback-guard case, and the lifecycle-capture case, which produced eight frames that all passed
edge probes. These results qualify
those contracts only; full application,
ARM64 consumer, assistive-technology, IME and hardware qualification remain separate gates. Retained Release paired
sets preserve the initial Retention flag (+11.85%) as unconfirmed; the unchanged-source repeat was within-noise with
unstable controls and did not establish that there is no performance cost or demonstrate an improvement. The hosted
Default flag also remains unconfirmed after a retry with no flagged metrics and unstable controls; neither run
establishes a confirmed regression or demonstrates a gain.

## Contract

DxUi Grid providers expose `IGridProvider` row/column counts and `GetItem` for each valid model cell. Requests use the
published stable row identity and materialize at most one offscreen row per request. The host retains no more than the
16 most recently materialized offscreen rows in each immutable snapshot; evicted cell providers report
`UIA_E_ELEMENTNOTAVAILABLE`. Visible rows and up to 256 selected offscreen rows keep their existing snapshot behavior.
The operation never scrolls, selects, or focuses a row.

Native HWND requests from a foreign thread use the existing bounded owner-thread UIA action dispatcher. Embedded hosts
allow read-only snapshot hits from any thread but require the owner thread to materialize an uncached offscreen row;
foreign-thread misses return `RPC_E_WRONG_THREAD`. Embedded materialization adopts a current snapshot only while the
attached root, requested Grid lifetime, and screen placement remain the same. Model callbacks can rebuild controls or
shrink the same model, so the materializer must recheck the host, control lifetime, model-assignment generation, row
count, and stable row id after callbacks, and abandon a superseded or partial snapshot publication. Assigning a Grid
model increments its assignment generation even when the pointer value is unchanged; retained cell providers are
invalidated by a new assignment at the same address. Named allocation and model callback exceptions are contained at
the provider boundary and become `E_OUTOFMEMORY` or `E_FAIL`.

## Implementation and validation

- [x] Add the GridPattern COM interface to the existing provider and keep counts snapshot-backed.
- [x] Cover a real UIA client, an arbitrary offscreen row, embedded owner-thread and foreign-thread behavior, invalid indices,
  cache eviction, detach, root replacement and same-model row shrink during a requested cell read, same-address model
  reassignment, and exception containment.
- [x] Run the x64 Debug, Release and ASan Debug suites: all 19 requested noninteractive suites passed in each profile;
      NewControls recorded 10 existing capability skips per profile.
- [x] Complete ARM64 Debug, Release and ASan Debug native profiles; the consumer scope below remains separate.
- [x] Retain same-fixture Release comparisons, including the initial unconfirmed flag and within-noise repeat with
      unstable controls: [qualification evidence](../../../Measurements/ViewerSpace/2026-10-08/README.md). The evidence
      establishes neither a confirmed regression nor an improvement and does not rule out a smaller cost.
- [x] Qualify the identified x64 RedSalamander consumer candidate: the analysis-host detached-provider/callback-guard
      case and lifecycle-capture case passed; the lifecycle case produced eight frames, all with passing
      physical-144-DPI edge probes. This is not full application or ARM64 consumer qualification.
- [x] Update the normative UIA spec and control catalog in the implementation change. There is no visual gallery change.
