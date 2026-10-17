# Unicode core migration evidence, 2026-10-17

The bounded document/projection/layout core is canonical in DxUi.lib. This is
D2's reusable source step, not a retained RichTextEditor or full Markdown/input/
accessibility/performance acceptance. Exact source and binary identities and all
57 native receipts are in [core-qualification.json](core-qualification.json).
The shared receipt-format benchmarkInputs field hashes the actual functional
test fixture sources; this record contains no invented timing measurements.
The parent commit plus recorded working-file hashes identifies the tested tree;
these pre-commit receipts are not restamped as a later clean release pin.

All 19 normal suites passed with zero capability skips in each x64 Debug,
Release and ASan Debug profile. The ten named rich-core scenarios run inside
EditorControls and include actual borrowed-target rendering on two independent
WARP devices. The ASan intentional fault produced heap-use-after-free. All three
ARM64 configurations built; native ARM64 execution is unqualified. Five validators
and tooling passed; the retained validator output is the complete final summary,
not the native child tooling transcript. Pinned clang-format 22.1.3 passed.

The initial WIL message-format warning and test's SDK small-macro collision were
fixed; both failed build logs remain separate from successful current receipts.

The existing-control benchmark uses the unchanged Default fixture and original
[pre-change baseline](../2026-10-17-prechange/README.md). Both the
[candidate](existing-controls-candidate.json.comparison.json) and one
[quiet repeat](existing-controls-repeat.json.comparison.json) return
**advice-required**, not a performance pass. The first run flagged clean memory
and dirty timing; the repeat instead flagged a sub-microsecond clean preparation
value and dirty private peak (29,351,936 to 29,949,952 bytes, about 2.04%). The
warning set changed; no causal regression or accepted non-regression is claimed.
No baseline was replaced and no further repeat was used to fish for a pass.
D4c's controlled paired study, attribution and owner advice remain open before
accepting a confirmed regression. The fixture does not instantiate rich text.

The exact-pin prototype relink/public consumer gate passed for code commit
302f3783a5d2ef60f201ca3abf508c1158fde039.
[Consumer receipts](consumer-qualification.json) bind actual archive/executable
identities and independent application smoke; these core-suite receipts do not pretend to
be executions against a different consumer archive. Focus-taking Menu,
NativeTextInput and menu resource fixtures remain explicitly unrun, as do physical
IME/AT and complete retained-editor resource/device-loss gates.

The [actual standalone consumer capture](prototype-consumer.png) was produced by
the pinned Release app's offscreen Direct2D/WIC harness, visually inspected and
has the same SHA256 as the pre-migration prototype. It is a consumer-host image,
not a screenshot of a retained DxUi RichTextEditor or a gallery support claim.

Prototype Debug/Release each pass 9/9; ASan passes 4/4 native application suites
and the specific intentional-fault diagnosis. ARM64 Debug/Release/ASan builds
pass without a native execution claim. The public consumer/revision-4/helper
headers pass, including ten mismatch rejections. Ownership, cross-runtime and
unsupported ASan configuration rejections are retained. The first SDL-poisoned
application probe failure is separate from its corrected successful receipt.
The consumer's clean code pin remains fixed while feature-branch documentation
and evidence advance; restore never resets an existing developer checkout.

[Final validation closeout](closeout-validation.json) retains the complete
stdout/stderr from the later full run. That run caught a missing ownership/hash
envelope on the functional source-identity record. The metadata was corrected;
all five validators were rerun successfully, and the full tooling run passed.
The earlier failed full-run status is retained, not relabeled as exit 0. Native
code/build inputs and the qualified source fingerprint stayed unchanged.
