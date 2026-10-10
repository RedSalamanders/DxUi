# Iteration 40 native qualification receipts

These receipts retain the source identity verified before the Git receipt-preservation attributes changed. The
[final iteration 42 record](../NativeQualification42/README.md) supersedes their current-source accounting; neither
the original receipts nor their hashes have been relabelled.

The lead independently verified all 57 whole-scope receipts against the current compiled-source, installed x64
dependency, executable/library/DLL/PDB closure, environment and profile identities on 9 October 2026. Each
corresponding raw suite report has exit code zero, zero capability skips, native architecture X64 and an executable
hash matching the current file. [The verification manifest](verification-manifest.txt) retains the identities and
hashes. [The sealed qualification archive](native-qualification-40.zip) contains the original JSON manifest,
build receipts and 57 pairs of scope receipts/raw reports in their three profile directories, without relabelling
them. These runner qualification records are distinct from benchmark measurement receipts.

Debug40b reused only Foundation's exact current-source Debug39 result and executed the other 18 scopes.
Release40b and ASanDebug40b executed all 19 scopes. ASan's isolated detection probe passed. The five validators,
complete tooling runner and clang-format 22.1.3 check passed; tooling was reused across profiles only with equal
independent identity. A later prose/evidence update requires a fresh tooling receipt, not identical native reruns.

Canonical ARM64 Debug, Release and ASan Debug cross-builds passed in iteration 38. No native C++ input changed
after those builds; subsequent changes affected tooling fixture cleanup and receipt identities. Cross-building
does not qualify native ARM64 execution, which remains NOT RUN.

Menu, NativeTextInput and the two Menu resource fixtures remain separate desktop obligations. Debug39's lease
confirmation timed out before execution. The new IMM regressions are compiled but their current desktop run is
pending. Real IME, assistive technology, touch, native ARM64, consumer adoption and policy migration retain their
separate status. The unpaired complex-UI samples included in these reports are reporting evidence; they do not
establish paired performance non-regression or production readiness.
