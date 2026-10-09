# Noninteractive native qualification snapshot (iteration 42)

A later IMM test-fixture correction supersedes this exact compiled-source identity. The original successful
receipts remain unchanged; [iteration 45](../BuildQualification45/README.md) records fresh six-profile builds and
the still-open current-source runtime gates.

All 19 noninteractive scopes executed and passed in x64 Debug, Release and ASan Debug after the receipt-preservation
attributes changed the conservative source identity. Each profile recorded zero capability skips. The ASan detection
probe, five validators, complete tooling runner and clang-format 22.1.3 check passed. Source, dependencies and actual
artifact bytes remained stable. The lead independently recomputed all 57 whole-scope receipt identities and verified
their raw reports, zero exit codes, native architecture and current executable hashes.

[The verification manifest](verification-manifest.txt) retains every identity and SHA-256. The
[sealed archive](native-qualification-42.zip) preserves the original manifest, build receipts and scope reports as
JSON in their three profile directories. The additional Release MenuTextLayoutResources fixture passed with zero
skips through the noninteractive runner; its report is retained separately in the archive. Runner qualification
records are distinct from benchmark measurement receipts.

This refresh changed Git receipt attributes, not native C++ behavior. Iteration 40 remains preserved under its
original source identity. ARM64 Debug, Release and ASan Debug cross-builds from iteration 38 retain unchanged native
inputs; they establish buildability, not native runtime qualification. A subsequent prose/evidence update requires
fresh independent tooling accounting. Staging or committing identical working-tree bytes preserves native identities.

Menu, NativeTextInput and the two focus-taking Menu resource fixtures remain separate desktop obligations until
their current reports pass under the restoring lease. Real IME, assistive technology, touch, native ARM64, exact-pin
consumer integration and Release policy migration retain their separate gates. Unpaired complex-UI samples included
in these suite reports do not establish performance non-regression or production readiness.
