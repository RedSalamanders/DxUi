# DirectWrite lazy metrics boundary

Implementation `95d5220e1c9ae8151ae71050a2ed5eda790b2d09`, qualified at
`4ce9b6a9534d869c4f670eced10e773e2cae5ea8` (API 5), puts only the opaque DirectWrite `GetMetrics`
call inside the audited native allowance. DirectWrite may handle its own lazy-formatting exception internally.
An escaping native exception or an SDK exception still follows the application-owned fatal protocol, and a
returned failure HRESULT retains checked refusal. The thunk is deliberately not `noexcept`.

The [qualification ledger](qualification.json) binds 685 actual canonical-checkout input files, six builds
with zero warnings/errors, all 22 ordinary suites in each x64 profile and actual ASan detection. Every original
source, native and run archive file member was independently reverified, including each executed suite image.
Empty ZIP directory entries are separate from file members. The maintained source/caret regression exercises
a 65,536-unit line, finite layout bounds and a caret at unit 65,000. Default gallery pixels are unchanged.

The owner's 7 November delivery scope is x64 Debug/Release/ASan runtime and all three ARM64 compilation
profiles. No ARM64 runtime was executed. The complete-checkout static validation and format check passed.
The complete checkout's original CRLF test-inventory bytes and the canonical Git checkout's LF bytes have
different compiled-input identities; the ledger records both and uses the canonical checkout for native results.

The [counterexample ledger](counterexamples.json) retains the original consumer fatal and the relink of its
unchanged objects with only the corrected SDK layout object. The oversized-math case then completes; the
independent image-watch defect remains an ordinary failure. Its original images, PDBs, exception context,
compiler objects and run evidence are preserved privately.

The first local link failed with LNK1140. All 131 original objects link successfully when only their output
locations are relocated and the exact native tool environment is restored; the focused regression passes.
No PDB policy changed, and the precise filesystem failure remains unassigned. The first static wrong-seed
fixture failure, sparse-checkout setup refusal and their raw output remain retained; the later complete-checkout
pass does not retrospectively diagnose them.

Durable behavior belongs to [failure handling](../../Specs/Core/Core_FailureHandling.md),
[validation](../../Specs/Testing/Testing_Validation.md) and [hosting](../../docs/hosting.md). Broader element
admission, current application adoption, foreground/physical checks and formal paired performance stay with
the [active parent](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
