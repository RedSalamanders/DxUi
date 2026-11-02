# Failure handling and allocation

Status: normative intended contract
Policy decision: 2026-11-02

This contract owns C++ failure handling in DxUi, including application callbacks invoked by the library. The owner
requires explicit checked failures or a fatal host diagnostic path. `std::bad_alloc` is unexpected, like any other
escaping C++ exception; catching it and continuing is not the ordinary allocation-failure protocol.

**Implementation status:** published source `5fc4d4c383bfa4891ee57a138924107bbbf11a2e` does not yet conform.
It includes throwing allocation/parser paths, exception-to-HRESULT recovery and callback suppression. The
[implementation and test migration plan](../Plans/WIP/FailurePolicy_2026-11-02.md) records the gaps. Historical
functional receipts establish their original behavior, not acceptance of this policy. No fatal hook or dump
guarantee is claimed to exist by this document.

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

## Unexpected exceptions and noexcept

No C++ exception may unwind through Win32, COM, callback or module ABI boundaries. A mandatory compatibility
catch is a terminal containment boundary, not permission to continue. Catch only named types, keep its fatal
reason/context, and enter the host fatal path. Broad or empty catch-and-continue handlers are prohibited.
Consumer callbacks called by DxUi must report expected refusal through their documented return values.
Named standard catches alone cannot cover an unknown application-specific exception type. `catch (...)` remains
prohibited; the host must prove a terminal runtime/crash route for unknown types with failure-site context captured
before unwinding. A plain noexcept/terminate fallback without that proof is not conformance. Keep the integration
gate open if no supported mechanism meets both the catch restriction and the context/dump requirement.

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

Use the host's prepared crash infrastructure to attempt the dump with the captured failing-thread context and
matching EXE/DLL/PDB identities. Dumping after unwinding and capturing only the terminate handler's stack is not
proof of the original failure callstack. Microsoft recommends a separate dumper process where possible, or a
dedicated thread, and supplying the previously captured thread state; DbgHelp access must be serialized.
[MiniDumpWriteDump documentation](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump).

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
the existing implementation. Keep the migration plan open until code, child diagnostics and relevant native,
platform and consumer acceptance are proved under their owners.
