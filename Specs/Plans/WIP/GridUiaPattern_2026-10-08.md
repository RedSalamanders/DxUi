# Grid UI Automation GridPattern

## Status

Source is implemented. On x64, each of Debug, Release and ASan Debug passed all 19 requested noninteractive suites. The
NewControls suite recorded 10 existing capability skips in each profile. ARM64 Debug, Release and ASan Debug cross-builds
also passed. Two retained Release paired sets did not confirm the initial retention-scene timing finding. Native ARM64 runtime and consumer capture qualification remain pending;
this WIP plan is not ready for closeout.

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
- [x] Complete ARM64 Debug, Release and ASan Debug cross-builds; these are not native runtime passes.
- [x] Retain same-fixture Release comparisons, including the initial finding and quiet repeat: [qualification evidence](../../../Measurements/ViewerSpace/2026-10-08/README.md). The repeat establishes no regression, not an improvement.
- [ ] Complete native ARM64 runtime qualification and consumer
      capture qualification before moving this plan to `Specs/Plans/Done/`.
- [x] Update the normative UIA spec and control catalog in the implementation change. There is no visual gallery change.
