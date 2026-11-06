# Explicit failure results and host fatal diagnostics

Status: **IMPLEMENTING**
Owner: DxUi maintainers; application owners for host dump integration
Priority: high; required before claiming the new failure policy is implemented
Reviewed baseline: `5fc4d4c383bfa4891ee57a138924107bbbf11a2e`

## Scope and owner decision

The 2 November owner request establishes the policy: `std::bad_alloc`, like any unexpected C++ exception,
must not be ordinary recovery. Check nonthrowing allocation results and return an error when safe, or terminate
the host through a diagnostic path retaining a useful original callstack and dump. The owning
[failure contract](../../Core/Core_FailureHandling.md) expresses this target. The owner's subsequent **go** authorizes
the library implementation, public host integration example and native fault qualification below. Runtime edits are
in progress; no new-policy native acceptance is claimed until the recorded checks pass. Actual consumer product
adoption remains owned by each application and is not implied by library qualification.

The published baseline still uses exception-based parser validation and allocation/callback recovery. Its
earlier functional receipts remain valid for their original source/contract; they do not qualify this new policy.
The original 21 delivery plans remain historical Done records. This separate requirement must not be hidden by
reopening their historical checklists or relabeling old allocation sweeps as fatal-diagnostic evidence.

## Verified findings

### F1 — Replace normative catch-and-continue promises

- **Evidence:** `Specs/Rendering/Rendering_Win32Host.md:42-47`, `Specs/Rendering/Rendering_EmbeddedD3D11.md:160-163`,
  and `Specs/UI/UI_InputAndAccessibility.md:816-825` at the reviewed baseline explicitly keep the host usable after
  standard exceptions or map thrown allocation to HRESULT failure. `Specs/UI/UI_ControlsAndLayout.md:2363`
  calls `bad_alloc` retryable; `Core_PerformanceAndResources.md:965-970` requires propagation/recovery sweeps.
- **Trigger/impact:** an unexpected allocation/callback exception is reported as ordinary refusal, losing the new
  fatal-policy outcome and original fault context. The contract itself asks tests to accept that behavior.
- **Confidence:** high, static source/spec inspection. **Effort:** small for documentation, large for runtime migration.
  **Fix risk:** high; changing failure continuation affects public behavior and test oracles.
- **Fix:** make the central classification authoritative, reconcile explicit conflicting clauses, and migrate the
  implementation/tests before declaring conformance. Generic allocation-failure wording means checked status, not a caught throw.

### F2 — Replace recovery helpers and allocating diagnostic suppression

- **Evidence:** `src/Controls/DxUi.Internal.h:206-235` catches standard callback exceptions and silently preserves
  old callable storage on `bad_alloc`; `src/Rendering/Embedded.cpp:124-127` maps thrown allocation to E_OUTOFMEMORY.
  `src/Support/Diagnostics.h:24-35` allocates via formatting and swallows standard exceptions.
- **Trigger/impact:** failed allocation or callback execution returns to the UI, and the ordinary logger cannot
  reliably deliver fatal diagnostics during OOM. Wrapping that logger in another catch is not a fatal bridge.
- **Confidence:** high, representative production paths opened. **Effort:** large for the full ownership/call-site inventory.
  **Fix risk:** high; publication, event ordering and reentrant cleanup must remain correct on checked refusals.
- **Fix:** separate checked resource refusal from unexpected exception containment, add the canonical host fatal
  channel, then migrate callers in bounded slices. Inventory all remaining catches; this review is not that full inventory.

### F3 — Cover the complete construction allocation closure

- **Evidence:** `include/DxUi/DxUi.h:1825` constructs Control's shared lifetime token with `make_shared`;
  `Separator` at `include/DxUi/DxUi.h:2111` and `src/Controls/DxUi.Controls.cpp:2727` inherits that work.
  The existing canonical `DxUi::CreateControl` factory in `src/Controls/ControlCatalog.cpp:45-93` wraps
  `make_unique` and catches bad_alloc as E_OUTOFMEMORY; that is legacy exception recovery, not a checked seam.
- **Trigger/impact:** `new (std::nothrow) Separator` still permits a nested constructor allocation to throw.
  Adding a null check to the outer object, or changing `noexcept` alone, does not meet the policy.
- **Confidence:** high, actual member initializer and constructor inspected. **Effort:** medium for the first
  canonical control path, larger across container/proxy allocations. **Fix risk:** high; lifetime-token ownership and ABI.
- **Fix:** establish checked factory construction or allocation-site fatal routing, cover shared control blocks and
  Debug STL proxies, and only then make the corresponding constructor/error contract truthful.

### F4 — Replace normal parser exceptions with explicit diagnostics

- **Evidence:** `src/Controls/DxUi.YamlMetadata.cpp:29-76,100-122` deliberately throws/catches private syntax/capacity
  types; its budget allocator also uses throwing `operator new`. `src/Controls/DxUi.MarkdownMetadata.cpp:61-83`
  catches toml++ `parse_error`. `src/Controls/DxUi.MarkdownSyntax.cpp:20-60` throws for ordinary admission failures.
- **Trigger/impact:** applying a blanket fatal catch to these sites would crash the application for malformed user
  YAML/TOML, which must remain a normal diagnostic. Keeping them as routine throws would violate the new policy.
- **Confidence:** high, parser callback and catch paths inspected. **Effort:** large, including backend/interface qualification.
  **Fix risk:** high; syntax/capacity distinctions, exact source and pinned corpus results.
- **Fix:** prove a supported nonthrowing backend result path or replace/adapt the backend safely. Do not assume a
  build flag creates such an interface, convert syntax errors to fatal errors, or longjmp across live C++ objects.

### F5 — Prove original-site diagnostics through the host

- **Evidence:** `include/DxUi/Diagnostics.h:5-20` has optional borrowed thread-local logging/performance sinks,
  not a fatal hook. No crash/terminate/new-handler installation was found in the scoped DxUi C++ search.
  The inspected consumer sources have existing crash writers: RedSalamander's `RedSalamander/CrashHandler.cpp`
  calls its dump writer with null exception context from `TerminateHandler`; RedXe's `RedXe/CrashHandler.cpp`
  has the same pattern. RedTilde's `Common/CrashHandler.cpp` and RedPrism's `Common/CrashDumps.cpp` raise a
  synthetic SEH exception in their fatal handlers. These are reuse candidates, not original-throw-stack proof.
- **Trigger/impact:** a terminate-handler stack can describe the handler after unwinding instead of the allocating
  or throwing caller. A bare fail-fast can bypass the installed unhandled filter. A dump file alone is insufficient.
- **Confidence:** high for inspected paths; no real crash or dump behavior executed. **Effort:** large across hosts.
  **Fix risk:** high; process-global ownership, recursion, loader/allocator state, worker threads and symbol identity.
- **Fix:** define the allocation-free handoff and context lifetime; capture at the failure site; reuse each host's
  crash infrastructure after qualifying its context/dump behavior. Do not install global hooks from DxUi.

### F6 — Migrate tests that require thrown-failure recovery

- **Evidence:** `Tests/Controls/DxUi.Tests.Controls.cpp:1661-1664` throws `bad_alloc`/`runtime_error` and requires
  HRESULT recovery. `Tests/Controls/Controls.Tests.IntrinsicLayout.cpp:178-179` uses a throwing measurement
  callback. `Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h:29-35` injects `bad_alloc` through global-new
  interception; `Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h:116-154` requires continued input dispatch.
- **Trigger/impact:** retaining those success oracles would contradict the new fatal contract; simply deleting
  them would lose failure coverage and could conceal mutations, double publication or missing dumps.
- **Confidence:** high, representative tests opened. **Effort:** large alongside the production conversion.
  **Fix risk:** medium/high; deliberate child faults must not reach the runner or desktop consumer.
- **Fix:** inject null/status at real checked seams for recovery tests; move unexpected throws and fatal allocation
  to owned isolated child probes with nonzero/no-continuation/context/dump oracles. Keep all earlier failed/pass receipts historical.

## Ordered implementation and acceptance

### Working-tree progress, not acceptance

- The `af7783aa` source passes six zero-warning builds and three clean-pin consumers. Its annotation-enabled
  consumer passes fifteen of sixteen fatal probes; the remaining child exits during host startup with
  `ERROR_ALREADY_EXISTS`, before reaching its fault. The retained PID directory predates that child. The
  example and oracle now use PID plus process creation time; scenario seven seeds a legacy PID-only directory
  before child resume and verifies its original bytes alongside the independent new dump/context checks.
  The original fixture, binaries, symbols, capture and failed result remain preserved. Fresh qualification is pending.
- At `f555c00c`, all six builds, four clean-pin consumers, isolated SDK and all 66 ordinary x64 suite executions
  pass. The later tooling-only integration repairs its changelog shape and shared-license source contract;
  these original passing receipts are retained under their actual source.
- Current main `86c62873` is integrated at `f6938257`, preserving the checked/fatal policy alongside its newer
  input, editor, parser, image and diagram behavior. Five source validators and the SDK notice controls pass.
  All six native builds and the production SDK build reject a merged inline-code spacing query that applied
  HRESULT checking to WIL's void-returning throwing overload. The explicit nonthrowing query now preserves
  the native status; fresh compilation/runtime qualification is required. Failed logs remain private build-11
  evidence. No native test or performance measurement ran from that rejected cohort.
- The `d0179db6` cohort passes all six zero-warning builds, four clean-pin consumers, isolated production SDK,
  complete validators/tooling and all 66 ordinary x64 suite executions with zero skips. Its first ASan foreground
  lease passes Menu and both resource suites, but NativeTextInput fails in the text-store callback child before
  producing its expected fatal record. Matching binaries/symbols, complete run and captured child output remain
  preserved in private `failed-foreground-10` evidence. Foreground/focus are unchanged and the pointer is restored.
  Source review finds that the guarded child reused a fixture requiring real HWND focus; the replacement stages
  logical focus without an HWND and invokes the real text-store edit/callback. Its existing attached-window
  healthy scenarios remain. This repair is source-reviewed only and needs compilation/runtime evidence; no
  cause is assigned to the earlier failure solely from that source review.
- Incremental ASan repair diagnostics pass Foundation (`20261103T050843427Z-108132-cf82204ad36f435f84d5a7d413db71ba`)
  and the focused rollback/native-paint cases (`20261103T050249663Z-120424-04ef048d62034e58916221bc833322f8`).
  The preceding Foundation diagnostic rejects VCAsan's truncated absolute dump filename; using a short name in
  the owned child working directory passes the exact dump/thread/stack oracle. The synthetic reader fixture
  uses an explicit thread-list header/entry layout because the Windows SDK exposes a zero-length array member.
  These are diagnostic builds, not final profile receipts. Static test-injection review maps 83 throw sites and
  all 58 allocation-countdown activations: 44 belong to registered fatal children and 14 are allocation-free
  negative guards. The callback no-copy negative guard executes no throw on its healthy path.
- The `6ebc4534` cohort passes all six zero-warning builds, all four clean-pin consumers and the production SDK.
  Its prose-only successor `d10aea19` has the identical compiled input digest. The complete validator/tooling run
  and all 22 ordinary Debug and Release suites pass with zero skips. ASan run
  `20261103T034748712Z-28744-2c4d212e43a444ff99c8167d167848cd` fails Foundation, EditorControls and Rendering;
  all matching binaries, original logs and failed/empty dumps are preserved under private `failed-asan-9` evidence.
  The real throwing-new child reproducibly aborts inside ASan before the C++ handler. Existing VCAsan dump output
  preserves its original allocation stack; a canonical reader bug treating zero stack RVA as file-header bytes is
  corrected with synthetic fallback/refusal coverage. Its ASan oracle is explicitly sanitizer-owned, not host or
  new-handler proof. The unchanged rollback child and complete Rendering suite pass focused replays; these do
  not establish the original failures' cause. Shared child capture now retains output and bounded timeout evidence,
  and the batch fixture explicitly covers preparation-only paints before judging one admitted failure frame.
  These source changes still require fresh compilation and runtime qualification; no failed receipt is retagged.
- The `fe34d0bf` cohort passes all six zero-warning builds, all four clean-pin consumer configurations and the
  extracted production SDK's producer-PDB isolation and both negative controls. The ASan annotations-disabled
  consumer passes all 16 fatal origin/dump/exit probes; the earlier timeout remains unassigned and preserved.
  PR 162's dependency validator finds a copied private-header path mismatch in the new driver. The fixture now
  preserves its canonical `Support/` include layout; all five source validators pass before the next freeze.
- All six `b2880fca` builds pass with zero warnings/errors, and Debug/Release/ASan annotated clean-pin consumers
  pass. The annotations-disabled ASan consumer times out at fatal scenario 9 after writing a successful record/dump
  for PID 108248. That child did not pass exit/origin verification; no cause is assigned. The driver now uses the
  canonical no-dialog setup, a pre-fail-fast event and separate live-child timeout capture while preserving 30 seconds.
- Independent extracted-SDK linking with the producer PDB unavailable exposes 70 LNK4099 warnings in the earlier
  `b2880fca` package. Its normal on-machine smoke receipt remains historical, not standalone symbol qualification.
  Adding the matching compiler PDB beside the extracted library passes the controlled link with zero warnings.
  Packaging now carries and hashes that pair; final isolated symbol qualification remains required.
- The two timing-sensitive tooling failures in `validate-complete-7.log` pass on individual repeats after native
  compilation drains: ConsumerQualification 13/13 and ToolingTestRunner 4/4. Original failures remain preserved;
  these repeats do not replace final complete validation after the remaining packaging/diagnostic change.
- The `6d24acf1` cohort passes all six warning-free native builds, SDK extraction, all validators/tooling and the
  affected Debug NewControls/EditorControls/Accessibility suites (`20261103T020826374Z-83400-ac928626b12e4f71ba84042d2e1afb8f`).
  Clean-pin consumers exposed missing COM include prerequisites and two const WIL guards in the new unload
  fixture. The isolated corrected fixture passes 12 healthy, two native, sixteen fatal and two unload probes;
  actual native publication requires its public `RefreshAccessibilitySnapshot` call. These test-only corrections
  are applied before a fresh exact-source cohort. Original failed build/runtime receipts remain preserved.
- Both first paired builds attested individually, but strict cross-side preflight rejected the historical baseline's
  missing `Samples/Shared` include path. No measurement slots were consumed. A reviewed study-only common build
  policy overlay aligns that include closure, preserves each side's library/project and retains the original refusal;
  both sides require fresh attestation. The unqualified acceptance policy/judge is unchanged.
- Final independent review found and repaired property-event admission after reentrant value/sender registration.
  Admission now checks the current attachment, original control identity and tree-item membership after each
  reentrant seam; a held-batch regression covers same-path replacement at all three stages and real-client values.
  Dedicated control-test STAs now remain initialized and pumping until process-owned UIA/GIT retirement succeeds.
  These repairs require fresh native/profile receipts; the preceding `1f69bbf` cohort remains historical.
- The same review traced a DLL callback-epilogue unload gap after the last admission scope releases. UIA delivery
  now transfers an owned DLL module reference to `FreeLibraryWhenCallbackReturns`; pending target ownership
  protects entry and Windows protects the eventual return. Executable callbacks skip the unnecessary unload pin.
  Two independently linked consumer DLLs exercise real queued UIA delivery, registration detach and physical
  unload while an active post-scope gate rules out an expired wait. This new fixture still requires execution.
- The focused property-registration regression passes at diagnostic run
  `20261103T015058979Z-87360-37680621d82c496da35928ad13ad78c9`, covering all three replacement stages.
  Diagnostic build 36 retained a test-helper compile failure caused by WIL's overloaded address operator; the
  helper now uses `std::addressof`. The library and Embedded executable built without warnings; these remain
  incremental diagnostic observations, not final canonical build qualification.
- The `1f69bbf` six-profile build cohort passed with zero warnings/errors. Its SDK extraction and three clean
  client configurations passed; Debug's cross-module fatal scenario 9 produced no record, while 15/16 did.
  The failed receipt is retained. Per-child stage/PID/exit diagnostics are added before a fresh run; an early
  launch/bootstrap refusal remains unproven and is not relabeled a dump-context failure or a pass.
- Final affected diagnostic run `20261103T005139880Z-92016-8864633cfa944074b594a331f0385c2c` passes
  Foundation, Embedded and Accessibility; EditorControls passed the preceding repeated diagnostic runs.
  These incremental Debug receipts do not qualify the final source or replace canonical/profile/consumer gates.
  Temporary native ref/lifetime tracing, selectors and lifecycle prints are removed for the next source freeze.
- Actual retained-root traces found two independent races: a notification worker could reopen a canonical root
  after detach, and owner disconnect discarded failed GIT revocation while delivery still borrowed its cookie.
  Acquisition now checks closure under the cache lock. Structure delivery shares the existing registration
  owner, releases its proxy before the lease, and queueing revalidates attachment after reentrant COM registration.
  Embedded disconnect drops stale event/diff baselines. The gated detach race and retained-source/identity
  assertions pass without clearing pins or forcing remote COM disconnection. Failed dumps/receipts remain retained.
- The no-activation fatal child now tests same-thread activation/focus hooks, keeping its foreground-preservation
  assertions, rather than issuing an unleased cross-process foreground request. Foundation lifecycle, original-frame
  fatal dumps and shutdown timeout/quit preservation pass. Test evidence prefixes include PID plus uptime to
  preserve old runs when Windows reuses a process ID; bootstrap errors report their captured Win32 status.
- Post-`7d1c627e` diagnostic runs pass EditorControls and Accessibility before the new retirement regression was added.
  Embedded's original fatal callback, stale typing/profile/HRESULT fixtures, SourceAnalyzer non-progress loop and
  image-loader cancellation expectation are repaired. A gated decoder test proves shutdown returns while decode
  is held and retirement releases it without exposing a late result. All failed runs remain retained.
- The actual Embedded DirectWrite exception was handled inside deferred `EndDraw`; every production `EndDraw`
  now uses the narrow native trampoline. Sample construction remains scoped, while OLE/COM lifetime and the
  message pump do not sit inside a blanket failure scope. Direct callback probes still require original-frame dumps.
- The final UIA shutdown diagnosis found an event sender retained after its local owner and client released it.
  Immutable runtime-ID and null HWND-host queries now work after retirement, allowing native cleanup while state
  and actions remain unavailable. The process-owned example performs final UIA cleanup before COM teardown,
  then explicitly retries host `BUSY` during cooperative bounded owner-message dispatch. It preserves quit status
  and cannot discard a pin or claim a hard deadline for arbitrary window procedures. The immediate focused
  scenario passes this path; new native/embedded retained-identity cases and all final profiles still need execution.
  Temporary refcount/lifetime instrumentation and the old diagnostic terminate override have been removed.
- Diagnostic run `20261102T235047537Z-6668-4e94c8393f4b4986a0162a5f02f94549` passes EditorControls; its new
  Embedded regression incorrectly navigated an old root after reattachment, and the native replacement oracle
  still rejected immutable IDs. Both test errors are corrected for the next build. This failed receipt remains historical.
- The protocol-2 `7d1c627e` cohort passed six warning-free builds, four clean-pin consumers (12 healthy,
  two internally handled native calls and 16 dump-unwound fatal cases each), the extracted SDK smoke, and all
  compiler-free validators/tooling. Its full Debug run failed Embedded, EditorControls and Accessibility;
  all failed logs, dumps and matching Debug binaries remain preserved. These results do not qualify later edits.
- The EditorControls child exposed a real unscoped direct context-menu callback: the fatal catch ran only after
  its original frame unwound. Entry scopes now cover this path and validated sibling dispatch/input entries.
  Five direct-entry children end their construction scope before calling the API; diagnostic execution verifies
  their original callback frames, and the original RichText context-menu child now passes without weakening it.
- The Accessibility dump proves the client was waiting inside `RemoveAllEventHandlers`; it does not establish
  the reason. The fixture now reuses the canonical bounded UIA client, checks stop/removal results and publishes
  completion after COM teardown, retaining its 20-second owner pump. Both targeted selection scenarios pass.
- A delayed public-API probe reproduced the Embedded typing failure: child startup/dump work split a one-second
  fence-trigger sequence. The child check now follows the healthy transaction. Another ported capacity fixture
  omitted its previous GFM profile; it now explicitly enables tables and checks preservation of prior publication.
  Further full runtime execution remains required. The common benchmark instrument now includes its own native
  allocation declaration so formatter include ordering cannot invalidate the paired candidate build.
- The `87820968` cohort passed all six warning-free builds, four clean-pin consumer configurations, the extracted
  production SDK smoke, and compiler-free validation. These receipts remain historical: subsequent runtime
  failures required source changes, so they do not qualify the current cohort.
- Actual Debug execution exposed internally handled Windows UI Automation exceptions under the first-chance
  observer. Failure protocol 2 now masks only an audited native call, re-enables observation for nested DxUi
  callbacks, and uses a POD-only SEH trampoline to capture an escaping C++ exception before unwinding. It does
  not whitelist a Windows module, exception type, or HRESULT. Independent-module probes now require actual dump
  unwinding for 16 fatal scenarios, plus two internally handled native calls and 12 healthy scenarios.
- Retained accessibility objects pin registration until their application-owned snapshots and callbacks have
  retired. Busy unregister leaves the admission gate open; optional final-release scopes acquire atomically.
  Focused race/native-boundary probes and isolated translation-unit builds pass, but canonical current-source
  builds and runtime qualification remain open. The failed UIA dumps and matching `87820968` binaries are preserved.
- Runtime review also corrected stale Grid selection/query expectations and RichText tests that still expected
  recoverable `logic_error`. Checked refusals assert unchanged state; convenience-contract violations retain
  isolated fatal children with originating-frame dump checks. Full runtime execution must verify these changes.
- The API 5 implementation includes explicit host/module registration, scoped fatal routing, checked outer
  control storage, transactional expected-error results, and an application-owned bounded dump writer.
  First-party unexpected exceptions are fatal; deliberate fault injections run only in owned children.
- The x64 Release Foundation diagnostic at `e2fc2350` passed lifecycle checks, 15 built-in fatal children and
  12 controlled TSF-association cases. The children include real checked native-null seams for control and YAML
  storage, unknown/standard exceptions, new-handler, worker, concurrent, missing/returning/recursive handlers,
  dump refusal and no-activation. Writable dump cases verify their saved context and named originating frame.
  This is diagnostic evidence, not a complete current-source build/runtime receipt.
- The current C-only YAML event interface passes the diagnostic corpus: 382 YAML and 717 TOML cases, two
  duplicate cases and 20 documented multi-document exclusions. Low-budget refusal/retry, directive/depth limits,
  public source limit and the formerly crashing long-tag input pass; 100 low-budget and three long-tag retry
  pairs retained the same process handle count. The private backend uses a live C-only refusal fence, a poisoned
  arena and native-null fatal routing; no jump crosses C++ objects or callbacks.
- All six `e2fc2350` builds compiled and linked without errors but emitted 26 vendor relative-include diagnostics
  per profile (duplicated in MSBuild's log summary). They are rejected under the zero-warning policy. A narrowly
  scoped include diagnostic has been added; the next frozen cohort must pass all six builds.
- Final source review found and repaired two native layout-result gaps: an ignored line-metrics probe and ignored
  drawing-state rollback failure. A real COM forwarder verifies E_FAIL/E_OUTOFMEMORY preserve the old publication
  and a healthy retry, and that failed setters restore actual effects/underlines. The prior library fails the
  preparation oracle; the corrected explicit object passes. Two fatal rollback children await canonical execution.
- The expanded public API 5 fixture compiles at /W4 /WX for x64 Debug/Release and ARM64 Release. True clean-pin
  multi-module consumer, complete x64 runtime/foreground/ASan, production SDK, current validator and matched
  resource/performance gates remain open. Actual application adoption is separately owned; native ARM64 runtime
  is explicitly deferred by the owner and must not be described as cross-build execution.

- [x] Review the published baseline, owner requirement, representative production paths and conflicting normative clauses.
- [x] Record the central target policy, current implementation gap and this indexed migration scope.
- [ ] **S0 — Freeze the full inventory.** Classify in-scope throws/catches, allocation mechanisms, public constructors,
  parser backends, callbacks and test injection sites. Capture source hashes and healthy-path construction/resource
  baselines. Record what returns a genuine status today and what depends on catching a thrown exception.
- [ ] **S1 — Qualify the fatal channel first.** Define a versioned bounded handoff with source/allocation category,
  failing-thread context and lifetime; reuse the host's dump owner. Prove no allocation/reentry on the failure path,
  missing/returning handler fallback, recursion and failure-to-write handling in child processes. A handler-time
  stack must not satisfy the original-site oracle. Source/PDB identity and native architecture are part of evidence.
  Include a non-standard application exception type: named std catches cannot cover every type, and the existing
  catch-all prohibition remains. The host's terminal runtime/context mechanism must be proved, not assumed.
- [ ] **S2 — Convert canonical allocation/construction.** Add/reuse checked RAII helpers and explicit fallible
  factories where errors can be returned. Handle unreportable failures at their allocation site. Start with Control
  lifetime storage and Separator, then shared/callback/container/proxy storage. A null-returning allocator must not
  violate an STL allocator's storage contract. Keep direct construction and factory semantics explicit.
  Reuse and correct `DxUi::CreateControl` before adding another control factory; audit the full nested initialization.
- [ ] **S3 — Convert expected input/capacity results.** Use explicit statuses for document/Markdown admission,
  malformed metadata, capacity, stale identities and unsupported notation. Select nonthrowing vendor interfaces
  only after inspecting their real error contract. Retain current bounds, complete corpus discovery, duplicate-key/
  alias/capacity distinctions and exact source. Invalid data must not become a fatal-process test.
- [ ] **S4 — Migrate boundaries and failure tests together.** Replace exception-to-continuation behavior in native,
  embedded, COM/UIA, callbacks, snapshots and diagnostics. Preserve atomicity and retry only for checked refusal.
  Migrate allocation sweeps to checked seams; isolate deliberate unexpected exceptions and fatal allocation probes.
  Reuse the canonical runner/failure-evidence infrastructure and register any new native cases in scope/inventory.
- [ ] **S5 — Integrate each consumer under its owner.** Reuse/qualify application crash writers and termination hooks;
  verify original-site context on main/worker threads and the real module/CRT boundary. No consumer repository or
  global handler is changed implicitly by this library plan. Review the API revision for changed source/failure
  behavior, update public compile/import fixtures and migration documentation, then adopt exact new pins.
- [ ] **S6 — Native/resource qualification and closeout.** Pass the applicable x64 Debug/Release/ASan suites, all
  six builds, isolated fatal probes and actual consumer gates. Native ARM64 remains an execution obligation,
  not a cross-build pass. Compare healthy-path construction, allocation counts, prepared/idle resource behavior
  and relevant Release workloads to the captured baseline; do not change the unqualified performance policy.
  Preserve original failures. Reconcile current docs, record actual source/artifact acceptance and move this plan
  to Done only when its implementation and qualification are complete.

## Validation, drift and stop criteria

Before implementation, compare the cited excerpts to the actual selected source and inspect independent dirty/
untracked changes. Useful drift commands are `git diff 5fc4d4c383bfa4891ee57a138924107bbbf11a2e -- AGENTS.md
Specs include src Tests docs`, `git status --porcelain`, and bounded `rg` searches for `throw`, catches,
`bad_alloc`, allocators and fatal mechanisms. The hashes and paths in this record are review anchors, not a live
inventory of consumer branches.

For this documentation review run `validate-skills.ps1`, `validate-specs.ps1`, `validate-dependencies.ps1`,
the other root validators, `Tools/Test-Changelog.ps1 -Base origin/main`, and `Test-Changes.ps1 -Explain`.
These validate guidance/links/accounting, not the runtime failure behavior. Do not create a test merely to repeat
the new prose. Actual implementation must use affected native coverage during each slice and final profile/native/
consumer accounting under [the validation contract](../../Testing/Testing_Validation.md).

Canonical ordinary final-profile commands, after the focused implementation slices, are:

```powershell
.\build.ps1 -Platform x64 -Configuration Debug
.\build.ps1 -Platform x64 -Configuration Release
.\build.ps1 -Platform x64 -Configuration 'ASan Debug'
.\build.ps1 -Platform ARM64 -Configuration Debug
.\build.ps1 -Platform ARM64 -Configuration Release
.\build.ps1 -Platform ARM64 -Configuration 'ASan Debug'
.\test.ps1 -Platform x64 -Configuration Debug
.\test.ps1 -Platform x64 -Configuration Release
.\test.ps1 -Platform x64 -Configuration 'ASan Debug'
```

These commands do not provide native ARM64 execution or foreground qualification. Run those obligations on their
required host/approved restoring lease. Schedule independent profile work under the canonical artifact/dependency
locks; interactive suites share one desktop lease, and paired measurements need their own admitted quiet window.
Fatal probes must use deterministic owned child failures and existing governed evidence roots, never physical
memory exhaustion, machine-wide WER changes or an interactive host crash. Acceptance includes the actual child
exit, no continuation, readable dump, originating function/thread and matching EXE/DLL/PDB receipts.

Stop a slice if the chosen backend lacks a safe status interface, a host cannot preserve failure-site context,
the fatal path allocates/returns, ownership is ambiguous, or the API/consumer port is not coordinated. Keep its
gate open; do not weaken the oracle or rename a caught throw to a checked refusal. Roll back code/pin changes
through normal reviewed commits, preserving fault evidence and historical packets. The owner authorized this
implementation and owned isolated-child tests; consumer process-hook adoption, publication and application product
qualification remain separate. Never inject a fatal fault into an interactive consumer.
