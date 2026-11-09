# Failure handling and allocation

Status: normative intended contract
Policy decision: 2026-11-02

This contract owns C++ failure handling in DxUi, including application callbacks invoked by the library. The owner
requires explicit checked failures or a fatal host diagnostic path. `std::bad_alloc` is unexpected, like any other
escaping C++ exception; catching it and continuing is not the ordinary allocation-failure protocol.

**Implementation status:** API 5 implements the versioned `Failure::HostServices` handoff, explicit module
registration and `Failure::Scope`, an application-owned sample reporter, checked control factory storage, and
checked document/parser results. Library qualification for the recorded x64 scope is bound to
`f6791b5070efd1fa8dc86601bc9422ae9a321d2e`; the [evidence](../../Measurements/FailurePolicy_2026-11-06/README.md)
and [execution record](../Plans/Done/FailurePolicy_2026-11-02.md) retain the build, corpus, fatal-child,
dump-context and isolated-consumer results. Native ARM64 execution and application-owned product adoption
remain separate. Formal paired performance qualification remains with
[D-P1](../Plans/WIP/RemainingWork_2026-11-04.md#d-p1); no passing formal verdict is claimed here.
Historical source `5fc4d4c383bfa4891ee57a138924107bbbf11a2e` used exception recovery and does not conform;
its functional receipts establish that original behavior only. No dump guarantee or product adoption is claimed.

The owner accepted the measured construction safety cost for this library scope. The descriptive matched
12-block study measured median increases of 10.355 ns for Separator, 2.4 ns for Panel and 16.285 ns for
RichTextEditor; tracked peak blocks increased by 39, 39 and 103 bytes respectively, with allocation counts
unchanged and no net live-byte increase. This is not a passing formal performance-policy verdict or a prediction
of consumer workloads. The evidence record retains its source identities and limitations.

ASan diagnostic builds retain the sanitizer's existing fatal-report owner. The MSVC sanitizer can terminate a
throwing allocation inside its runtime before the C++ new-handler runs. Such a result must be classified as a
sanitizer allocator fatal, with its own original-stack/dump evidence; it is not a new-handler or DxUi reporter
proof. Application owners enable the existing `ASAN_SAVE_DUMPS` facility in an owned output location when they
need these diagnostics. Do not replace VCAsan's report callback: that would displace Visual Studio integration.
Ordinary Debug/Release still qualify the actual new-handler path, and unexpected C++ throw probes retain the
DxUi host contract in every profile. See [Microsoft's VCAsan contract](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-debugger-integration?view=msvc-170).

The example host creates evidence directories using the process ID and creation time, queried through the
held process handle. A process ID alone can be reused while an earlier process's reports remain. Directory
creation remains checked and exclusive: never overwrite retained evidence on a collision. Parent-side dump
verification resolves the same process instance through its original handle, rather than reopening a PID.

## Failure classification

| Event | Required result |
| --- | --- |
| Invalid user data, unsupported notation, stale identity, cancellation or a documented capacity bound | Validate before mutation and return the owning explicit status/diagnostic. Preserve the documented transaction/source fallback. Do not use a C++ throw as routine parser or admission control flow. |
| A genuinely nonthrowing allocation returns null, or a native API returns an allocation-failure HRESULT | Check before use. If the operation has a safe failure result and staged ownership, return the error without partial publication; retry only under the owning bounded policy. |
| Checked allocation fails where the API cannot report failure or safe state cannot be maintained | Enter the host's fatal diagnostic path at the failure site; capture context, attempt the configured dump and terminate the host with a nonzero failure result. |
| Any unexpected C++ exception, including `std::bad_alloc`, `std::length_error`, `std::invalid_argument` or an application callback throw | Enter the fatal diagnostic path. Do not convert it into a successful fallback, retryable OOM, skipped frame, ignored callback or ordinary parser diagnostic. |
| A documented device-loss or other native failure code | Follow its explicit recovery/state contract. A returned HRESULT is not a thrown C++ exception. |

This policy applies to library C++ paths, not PowerShell validation exceptions or the test harness's deliberately
isolated assertion protocol. It does not authorize changes to a consumer's unrelated failure domains.

## Checked allocation and construction

API revision 5 consumers install the application-owned host before using scoped DxUi work. A module links and
registers its own copy of the immutable host services outside `DllMain`; its workers and objects must quiesce
before detachment. The installing thread performs host uninstall after stopping new entries. Direct stack or
smart-pointer construction of a control requires an enclosing host-owned `Failure::Scope`; the library does not
implicitly install process-wide hooks. See [the consumer contract](../Build/Build_ToolchainAndConsumption.md).

`CreateControl` checks the outer control-object storage through a null-returning allocation and reports
`E_OUTOFMEMORY` while preserving the caller's existing output. An unavailable host returns `CO_E_NOTINITIALIZED`;
with a configured host an unknown kind returns `E_INVALIDARG`. It enters a failure scope before constructing nested members, which do
not become checked merely because the outer allocation is nonthrowing. `Separator` remains `noexcept` under the
host fatal contract. Public fallible queries, document operations and Markdown preparation use their declared
`Try`/result APIs; a failed Markdown `Advance` reports its checked status without replacing the previous publication.
These interfaces and their concurrency, allocation and ABI boundaries still require the plan's native evidence.

Use one canonical owned helper/factory for each supported checked-allocation mechanism. Retain RAII ownership,
validate sizes/overflow before allocation, check null or the returned status immediately, and stage all storage
needed for a transaction before changing published state. A fallible `Try`/factory API must document both its
failure result and state guarantee. Never return a partially initialized control as success.
The refusal/reporting path must not need another heap allocation to return its error. Preserve the original
status through bounded diagnostics instead of allocating a formatted message while reporting allocation failure.

Ordinary `new`, `make_unique`, `make_shared`, strings and containers cannot be made fallible by adding a null
check after a throwing operation. `new (std::nothrow) T` checks object storage only; the constructor, base classes
and member initialization must also satisfy the chosen failure contract. The
actual allocator must be checked under the host's installed allocation policy; do not infer a null-result
contract from the spelling alone. The
[MSVC allocation documentation](https://learn.microsoft.com/en-us/cpp/cpp/new-and-delete-operators?view=msvc-170)
distinguishes throwing allocation from null-returning allocation; the
[C++ new-expression rules](https://eel.is/c++draft/expr.new#27) separately propagate an initialization exception.

For allocator-backed types without a genuinely nonthrowing status interface, either introduce a reviewed
checked representation/API or route failure at the allocation site to fatal diagnostics. A conforming STL
allocator must not return null where the container requires storage. Do not globally replace an application's
allocator from the library. Debug iterator proxies, shared-owner control blocks, callback storage and nested
construction belong to the allocation closure; checking only the outer object is insufficient.

Invalid input and parser capacity must remain explicit refusals even when a third-party backend currently uses
exceptions internally. Select and validate a nonthrowing error-result interface, or change the backend. Do not
turn malformed YAML/TOML into a host crash, quietly retain exception-based syntax control flow, or use an unsafe
`longjmp` across C++ objects to escape parsing.

The private YAML backend admits only borrowed-memory event parsing. Its C adapter
owns the live `setjmp` frame for each vendor entry, including text/tag access and
cleanup. A documented memory-budget refusal jumps only through C vendor frames,
poisons that parser, and returns `Capacity` to the C++ caller; no C++ owner or
callback is crossed. The arena then frees all remaining vendor storage without
running vendor destructors on partial state. This interface must not accept
streams, emitters, caller diagnostics, callbacks, locks or external resources.
Physical native allocation failure bypasses this refusal mechanism and enters
the host fatal path. The adapter enforces its event-depth bound independently of
graph admission, and completes cleanup before selecting the final result.

## Unexpected exceptions and noexcept

No C++ exception may unwind through Win32, COM, callback or module ABI boundaries. A mandatory compatibility
catch is a terminal containment boundary, not permission to continue. Catch only named types, keep its fatal
reason/context, and enter the host fatal path. Broad or empty catch-and-continue handlers are prohibited.
Consumer callbacks called by DxUi must report expected refusal through their documented return values.
Named standard catches alone cannot cover an unknown application-specific exception type. `catch (...)` remains
prohibited; the host must prove a terminal runtime/crash route for unknown types with failure-site context captured
before unwinding. A plain noexcept/terminate fallback without that proof is not conformance. Keep the integration
gate open if no supported mechanism meets both the catch restriction and the context/dump requirement.

Windows ABI implementations may raise and handle C++ exceptions internally before
returning an HRESULT. Such an internal event is not evidence that an exception
escaped into DxUi. Audited UI Automation calls, D3D11 device creation, deferred Direct2D `EndDraw` work and
DirectWrite's lazy `IDWriteTextLayout::GetMetrics` and `GetOverhangMetrics` formatting use `Failure::InvokeNative` with
pre-evaluated POD arguments. Its native allowance masks only the synchronous
foreign implementation; every nested DxUi scope reactivates observation for
reentrant providers and application callbacks. Never apply this allowance to an
application delegate or use it to ignore WIL exception types or whole modules.
The DirectWrite allowance covers only each opaque metrics call: source/projection allocation, publication and
subsequent SDK work remain observed. An HRESULT refusal must retain the previous prepared layout. The real
65,536-code-unit unbroken source regression must retain source and caret geometry; an internal Windows
formatting exception handled before HRESULT return must not become an application fatal report.
A separate noinline POD-only SEH trampoline catches no failure for recovery: its
filter sends an escaping MSVC C++ exception's original exception pointers to the
host before unwinding; non-C++ SEH continues its existing search. `/EHsc` remains.

Failure host protocol 2 includes the native entry/exit callbacks. All linked
modules must use the same table version and headers. Callback-capable UIA targets
and peers retain `Failure::ObjectLifetime` until their owned fields are destroyed,
including on another thread after control retirement. Host/module uninstall returns
`BUSY` while such peers, active entries or queued notifications remain. Release
retained interfaces and finish those callbacks before removing their host/module.
An atomic optional scope supports final retirement without a check-then-enter race;
an inactive optional scope permits only allocation-free, callback-free cleanup.

Public input and callback-dispatch entrypoints open their own failure scope before
staging work or invoking application code, including direct calls outside a window
message dispatch. Shared callback helpers also protect reentrant entry. A caller's
setup scope must not be the only protection: fatal-child tests finish construction
inside a setup scope, end it, and then call the public entrypoint directly. Argument
evaluation remains the caller's work; direct control construction requires its
documented enclosing host-owned scope.

Keep such application scopes limited to direct construction and application work. Do not put one around
`GetMessage`/`DispatchMessage`, COM apartment initialization/cleanup or an entire native message pump: that would
observe foreign exceptions outside the library's failure domain. Audited native calls inside scoped work use
the narrow allowance; the sample's OLE drag loop follows that contract.

The process owner shuts down UIA while COM and the reporter remain alive. Its explicit example
`Host::UninstallWhenIdle` gives asynchronous provider releases a cooperative five-second message-pump deadline,
retries only `BUSY`, preserves `WM_QUIT` and reports timeout. It cannot interrupt a blocking application window
procedure. Stop new work, application callbacks and registered modules first; neither this helper nor a view's
destruction permits removing a live provider pin. Component/DLL uninstall remains nonblocking.

Native view retirement also releases the module's idle animation-dispatcher HWND before the process reporter
is uninstalled. The dispatcher remains warm while another native host is attached and finishes an active timer
callback before destroying its window. Thread-local destruction must not invoke its scoped window procedure
after the host has gone. Applications satisfy this contract through ordinary native `ControlHost::Detach`;
they must not depend on a private dispatcher shutdown hook.

Shared COM registration leases extend through every asynchronous use and the resolved proxy's release. The
registering apartment must remain COM-initialized until GIT revocation; a retained module pin or a disconnected
view alone does not keep that apartment alive. Final cleanup must check the native revocation result; it cannot
discard a failed live registration. The sole final lease must own a valid cookie until revocation; any failed
revocation, including an invalid-cookie result, violates that invariant and enters fatal diagnostics. Retirement
cannot publish a new canonical root or COM registration after its target closes.

`noexcept` is a verified contract, not allocation error handling. Adding it to a throwing construction path can
terminate before useful fault-site diagnostics; removing it merely exposes the throw. Neither change alone
implements this policy. Review the complete base/member/factory path before changing `Separator` or another
control's declaration. Public factories and direct construction must both have a defined failure outcome.

Do not disable compiler exception support as a substitute for this conversion. Existing STL/vendor paths and
ABI containment remain until their implementation and tests change. A build-flag change requires independent
review; `/EHa` or a generic SEH recovery wrapper must not turn a hardware fault into continued UI operation.

## Host-owned fatal diagnostics

The application owns crash handling, dump location, symbols, process termination and any process-wide CRT/SEH
hooks. DxUi must use an explicit reviewed integration with that owner. Its ordinary diagnostic sink is not a
fatal channel: optional logging, formatting allocation and a returning callback cannot satisfy this contract.
The library must not silently install/replace `new_handler`, `terminate_handler`, global allocation functions,
unhandled filters or vectored handlers for an unrelated host.

The fatal handoff must use bounded, allocation-free records and capture the failing thread/context before stack
unwinding destroys the original frames. Include failure category, operation/allocation site, requested size when
known, thread identity and original exception/context information when available. Transport must respect module
and handler lifetime, avoid reentry and retain no borrowed pointers after their valid scope. Do not allocate
formatted strings, invoke ordinary observers or create a worker on this path.

The bounded live frame list identifies the capture path. An ARM64 SEH filter can
run on a handler path that does not contain the original throw frame. Verify
that record against the capture-site symbol and verify the original named throw
independently by unwinding the saved exception context with matching images and
PDBs. A capture-site match never substitutes for the original-context check.

Use the host's prepared crash infrastructure to attempt the dump with the captured failing-thread context and
matching EXE/DLL/PDB identities. Dumping after unwinding and capturing only the terminate handler's stack is not
proof of the original failure callstack. Microsoft recommends a separate dumper process where possible, or a
dedicated thread, and supplying the previously captured thread state; DbgHelp access must be serialized.
[MiniDumpWriteDump documentation](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump).

Static SDK distribution also retains the matching compiler PDB beside `DxUi.lib`, and binds both hashes in its
manifest/receipt. A successful link on the producer machine can silently borrow its original PDB path; header
dependency closure alone does not prove portable symbol closure. Qualification must link the extracted pair
without access to that producer PDB and retain the zero-warning result. Consumers keep their resulting executable
and DLL PDBs with the corresponding binaries for later dump analysis.

After the diagnostic attempt, terminate the host; the fatal handler must not resume normal execution. A missing,
returning, recursively failing or unavailable host handler requires a bounded platform fail-fast fallback carrying
the available failure record. `RaiseFailFastException` bypasses ordinary exception handlers and WER depends on
machine availability/configuration. Therefore, an unhandled-exception filter or a bare fail-fast/terminate/exit
call alone is not a verified application dump path.
[RaiseFailFastException documentation](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-raisefailfastexception).

Fatal termination is required; dump creation is best effort under actual memory/storage/system failure. The host
must bound the attempt and preserve its outcome when possible, without infinite retry, deadlock or a success exit.
Acceptance requires an actual valid dump and useful originating stack in the deterministic writable test scenario;
the impossible case of unavailable storage must still terminate predictably and report the dump failure where possible.

## Validation and compatibility

Checked-failure tests inject null/status at the real nonthrowing seam and verify the returned error, unchanged
publication/history/selection, ownership cleanup and any explicitly allowed bounded retry. Constructor coverage
must include nested allocation and MSVC Debug proxy paths. It must not simulate a checked refusal by throwing
`std::bad_alloc` and accepting a catch-and-continue result.

Unexpected exception and fatal-allocation tests run only in owned isolated children using governed evidence
paths. Prove nonzero termination, no continuation, bounded handling, matching binaries/symbols, a readable dump
and the named originating function/failing thread. Handler-time synthetic context alone fails the original-stack
oracle. Also cover unavailable/returning handlers, recursion, dump failure, concurrent failures and foreign-thread
entry. Never exhaust machine memory or crash the interactive consumer to obtain this evidence.

Preserve source/node/capacity bounds, valid-input conformance, lifecycle and rendering behavior. Update allocation
injection, parser diagnostics, input/COM tests and the consumer compile fixture with the implementation. Review
API revision changes for altered source/failure behavior; matching headers/archive/toolchain/CRT and a fresh
consumer pin remain mandatory. A documentation change does not increment the current runtime revision or qualify
the existing implementation. The recorded x64 library scope does not establish native ARM64 execution,
formal performance-policy acceptance or application-owned consumer qualification.
