# Read-only Source snapshot synchronization

## Checklist

- [x] Retain the pre-change native artifacts and exact merged source; production `src`/`include` at `619dc88b` matches the qualified SDK pin `2fed8246`.
- [x] Implement immutable source sharing with target-local revisions and the existing reset/selection notification semantics.
- [x] Pass both Release native/embedded source, history, Unicode boundary, UIA, source-ink parity, source-owner release and callback-retirement regressions; the ordinary-copy counterfactual fails exactly at the sharing assertion.
- [ ] Pass six build profiles, x64 Full Debug/Release/ASan and a real ASan detection probe; retain native ARM64 CI separately.
- [ ] Qualify exact-pin RedTilde adoption and its final paired comparisons; no performance gain is inferred from implementation.
- [ ] Publish evidence, update durable contracts/changelog, push and merge the qualified SDK revision.
- [ ] Move this plan to Done after its required gates pass; preserve parent performance/platform holds.

## Scope and contract

Focused continuation: the first Release case reached the bitmap check but its hidden fixture could not capture;
retain that failed run. The fixture now uses the existing non-activating display path and includes source-owner
release coverage. A separately built ordinary-copy counterfactual fails exactly at the source-sharing assertion;
its binary, receipt, source and failed evidence are retained. The restored implementation passes both filtered
native/embedded cases with no skips in run `20261026T010425257Z-47844-6abe4fba5b874e6186e9496e0bcf03f2`.
The negative run is `20261026T004418071Z-35148-fb86cdab43b34cef8b9db39fc656ada0`. The initial capture-setup
failure remains separately retained. The suite's unpaired frame timings do not qualify performance.

`RichTextEditor::SynchronizeReadOnlySourceSnapshot` lets a configured read-only Source control share an immutable
`SourceSnapshot`. It changes no class data layout, virtual method or API revision, and introduces no dependency.
Changed source clears only the target history, advances its own revision, retires the same derived state as
`SetText`, then publishes selection through the ordinary host path. The target parser remains independent.
Same-source selection uses the normal selection reset semantics; identical source/selection is idempotent.
Invalid snapshots or scalar/CRLF selection boundaries and incompatible target modes refuse atomically.
Like other setters, calls belong to the control owner thread; detached and embedded controls need no private HWND.

The RedTilde parent plan owns consumer policy, hidden-view deferral, callback supersession and performance
qualification. A successful library build or green hosted profile does not close the consumer's performance,
physical IME/AT or hosted ARM64 gate. The current proposal makes no syntax-sharing or total-memory claim.

## Validation and publication

Use affected testing while editing. Final qualification uses the canonical six builds and all three x64 Full
profiles, a real ASan detection probe, ordinary authorized foreground suites, exact-head hosted ARM64 and public
consumer compilation. Source glyph pixels must match an independent `SetText`/`SetSelection` oracle; a separate
plain-ink capture proves the comparison actually includes colored syntax. UIA must report exact read-only source.
Keep all failed attempts, counterfactual artifacts and source/receipt identities. No tolerance or baseline changes.

Gallery/design-system review: this additive source-storage API adds no control or intended visual change. Existing
design tokens and gallery captures stay valid; the native glyph parity regression guards that boundary.
