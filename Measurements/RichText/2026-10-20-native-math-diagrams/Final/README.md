# Native mathematics and diagrams: final local receipts

Runtime source: `778e8630392812c65da364c778056ff42b862f36`, on `ej/redtilde-math-mermaid` stacked above
PR #89's `29fc7897b21db3cd1d27f7ec52d6756fe9af36c0`. Later commits in this packet update documentation/evidence
only; they do not move RedTilde's immutable runtime pin. `SourceManifest.json` binds the actual library input
fingerprint, fixture hashes and six profiles' binaries.

All **19 noninteractive suites in x64 Debug, Release and ASan Debug pass with zero capability skips**: 57 suite
receipts. EditorControls, Embedded and Accessibility run first; the other 16 default scopes run separately to
complete the conservative PrePush fallback without duplicating those three. Native test-filter/watchdog and
private-desktop lease self-tests pass. The real AddressSanitizer expected-failure probe detects heap use after free.
All six x64/ARM64 Debug/Release/ASan builds pass. This host is x64; ARM64 results are cross-builds.

The final added geometry oracle compares actual prepared glyph positions for aligned equations, cases and
centered matrices; parser/native/embedded scenarios exercise opaque oversized fallback, source mapping,
rich/source selection and undo, cached native objects and state/class diagrams. The original Mermaid corpus
retains every 105-flowchart/20-family item and its support outcome. Unknown grammar remains exact source.

Every JSON adds library ownership, fixture input hashes and the runtime source binding to the original receipt.
Suite receipts retain executable hashes, capability skips and their original complex-UI measurements. Build
receipts retain their normal attestation outcome/identity/digest; the source manifest adds actual output hashes.
`Performance-*` reports preserve raw unpaired rounds and their full native provenance. They are diagnostics,
not accepted paired performance or a replacement baseline. Root-level pre-repair receipts/logs remain history.

`validate.ps1` passes all five validators and the complete tooling suite; the pinned formatting check passes.
The gallery was regenerated from the final Debug source with `gallery.ps1 -PublishDocs`; light/dark/rainbow/HC
and embedded sheets were visually reviewed. Documentation, the design system, capability status and the active
plan describe the bounded profiles. RedTilde's exact-pin adoption, recovery, app tests/captures and package
evidence remain in RedTilde's repository; no consumer workload is represented as an independent library fixture.

No focus-taking tests were authorized. Full LaTeX/Mermaid compatibility, native ARM64 runtime, physical IME/AT,
WARP/clean/dirty/hidden resource acceptance, D4c and release qualification remain open. PR CI builds its current
merge candidate, which contains upstream changes absent from this pin; pending CI and different-source passes
cannot replace the local exact-source evidence here. The overall rich-editor plan remains WIP.
