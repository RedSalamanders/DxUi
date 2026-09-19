# Localized layout — ARM64 cross-builds

`build.ps1 -Platform ARM64` succeeded for Debug, Release and ASan Debug, serially on this x64 host.
All library, test and sample targets compiled. These are cross-build results only; no ARM64 binary
was executed and native ARM64 runtime acceptance remains separate.

Retained logs identify the discovered Visual Studio installation and configuration. The text receipts
record the built library/executable SHA-256 hashes and sizes. Their compiled-source fingerprint is
the same source inventory measured by the adjacent final ASan resume: production source did not
change between that run and these builds. The source is HEAD `f7fe1e4` plus the recorded dirty
implementation. These receipts do not imply an immutable consumer pin has been adopted.
