# Explicit failure results and host fatal diagnostics

Status: **SPEC READY**
Owner: DxUi maintainers; application owners for host dump integration
Priority: high; required before claiming the new failure policy is implemented
Reviewed baseline: `5fc4d4c383bfa4891ee57a138924107bbbf11a2e`

## Scope and owner decision

The 2 November owner request is a normative-policy review: `std::bad_alloc`, like any unexpected C++ exception,
must not be ordinary recovery. Check nonthrowing allocation results and return an error when safe, or terminate
the host through a diagnostic path retaining a useful original callstack and dump. The owning
[failure contract](../../Core/Core_FailureHandling.md) expresses this target. This review changes documentation
and records implementation work; it does not claim the runtime, host applications or native fault tests were changed.

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
through normal reviewed commits, preserving fault evidence and historical packets. Broad allocator/ABI or
consumer changes require their own implementation validation; this review is not authorization to crash a host,
replace its process-global handlers or publish a new runtime.
