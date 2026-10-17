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

The exact-pin prototype relink/public consumer gate follows the first source
commit. Its actual archive/executable identity and independent application smoke
receipts are recorded separately; these core-suite receipts do not pretend to
be executions against a different consumer archive. Focus-taking Menu,
NativeTextInput and menu resource fixtures remain explicitly unrun, as do physical
IME/AT and complete retained-editor resource/device-loss gates.
