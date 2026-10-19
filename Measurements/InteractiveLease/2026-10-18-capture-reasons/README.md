# Desktop-lease focus capture diagnostics

This packet records the bounded test-harness diagnostic added after PR #86's
`0dbbad09e10d9e71873f2ac37b3b1765b33930b5`. It exercises library-owned fake desktops and
read-only Windows queries, without consumer repositories or services. Separate evidence rows record the
explicitly approved local x64 foreground runs under the restoring lease and its visible 30-second countdown.
The [qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#bounded-focus-capture-diagnostics-18-october-2026)
owns the remaining native-platform, foreground, resource, performance and product acceptance.

[Evidence summary](evidence.json) binds the changed source bytes, executable/receipt hashes and recorded outcomes.
The private `Z:/DxUiFocus` packet retains the original build/test logs, failed assertion attempts,
ordinary performance receipts, baseline receipt/artifact hashes, source snapshots and unrelated-work preservation audit.
Raw desktop identities are not published here.

Validation uses a clean checkout of the stated base plus the listed task files; unrelated local work is
excluded and hash-guarded. All six x64/ARM64 Debug, Release and ASan Debug builds pass. The three x64
noninteractive runs pass all 19 selected suites each, with zero skips and 45 InteractiveLease cases per
configuration. The PowerShell interactive-mode suite passes 36 cases. ARM64 builds are cross-build evidence.

The three approved local foreground leases all retain `recorded`/error zero and verify desktop restoration.
Menu passes all 110 cases in each profile. NativeTextInput passes all 165 Debug cases, fails Release case 84
after 83 completed cases at the IMM-preview assertion, and fails ASan Debug case 104 after 103 completed
cases at the independent-edit setup assertion. All six executions have zero capability skips. The Release
case passes once in isolation on the same executable; the original full failure remains retained and open.
The ASan setup trace records failed foreground activation before typing. Neither isolated success nor
successful restoration clears either failed child. The qualification ledger records the bounded next steps.

The retained pre-implementation Debug comparison flags seven timing metrics; one serialized repeat of the
same retained diagnostic candidate flags ten. Neither flags process memory, exact budgets or allocations.
These precede the final missing-result driver repair and do not qualify the final candidate. Library and
benchmark source fingerprints match, but baseline/candidate executable and v2 toolchain identities differ;
the initial candidate overlaps an ARM64 build. Baseline receipts/hashes are retained but its images were
not; candidate images are retained. A child-shell repeat launch is refused before sampling, then the
matching parent environment admits the one actual repeat. Both flag sets and these limitations remain
explicit; no repeat-until-green, causal attribution or paired non-regression is claimed.

The deterministic scenarios distinguish absent foreground, absent thread, minimized foreground,
failed query (including error zero), successful query returning null, and recorded focus. They verify
one eligible capture query, no diagnostic re-query, immediate error preservation, immutable capture
across confirmation/children, visible-focus restoration, failed read-back rejection, native formatting,
legacy/malformed parsing, honest presentation and the canonical runner's shared child-receipt mapping.

The new native fields are `focus.capture.reason` and `focus.capture.error`; child receipts copy
`lease.focusCapture` with `Reason` and `QueryError`. This bounded diagnostic has no HWND, process/thread
identity or event history. It does not change the capture target, minimized guard, preparation, warning,
restoration limits or meaning of focus `none`.

ARM64 builds from this x64 host are cross-build evidence only. The separately identified native ARM64
Debug hosted Menu/NativeTextInput observation remains open; historical `focus=none` receipts cannot
acquire a reason retroactively. A verified native focus round trip requires an initially visible
foreground with recorded live focus. All remaining qualification gates stay open, policy remains
`qualified=false`, and the 900 ms hover guard, 180 ms pointer bound, 5% timing/FPS and 2% memory bands,
exact budgets and allocation ceilings are unchanged. Ordinary benchmark receipts are diagnostic and
do not establish paired non-regression.

No production library code, public API or control pixels change. Test usage documentation and the
normative validation contract are updated; gallery regeneration is unnecessary.
