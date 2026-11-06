# Editor input and viewport verification

Frozen implementation: `efef569e839c640415f142bc860607b38e3ef138`, API 5.
The [original qualification](qualification.json) records six zero-warning builds,
66 successful ordinary native x64 suite executions, actual ASan detection and
independent byte verification of all 683 original inputs and every native/run archive member.
ARM64 entries are cross-builds.

[Retained counterfactuals](counterfactual.json) cover metadata-only composition pixels,
read-only shortcut propagation, Unicode pairing and disclosure/zoom viewport stability.
The unchanged viewport test object fails with the retained library and passes with the patch.
Disclosure keeps offset 7563.3 and reuses 200 chunks. Zoom keeps source-anchor Y=8 in both views.
Explicit caret intent and source replacement retain their own precedence.

The original matrix's compiler-free run failed one descendant readiness fixture (167/168).
Its historical cause is unassigned. The separately published deterministic readiness fix is
included in the [integration record](integration.json); all native code, headers and projects
remain byte-identical to the frozen matrix. Exact integrated-head compiler-free validation
is recorded in the pull request. The first malformed probe fixture and initial metadata
test setup assertion remain preserved, separate from accepted evidence.

There is no new foreground, physical IME/AT, native ARM64, consumer API-5 or formal paired-performance acceptance.
