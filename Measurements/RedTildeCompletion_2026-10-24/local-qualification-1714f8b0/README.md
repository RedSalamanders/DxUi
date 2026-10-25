# DxUi local qualification at 1714f8b0

This public projection records source-bound x64 runtime qualification and ARM64 cross-build evidence for `1714f8b0c0fa628bcef56de76078f85c869fc50c`. It contains every suite outcome with its reported executable digest and current-executable match, build attestation identities and digests, artifact hashes, PE architectures, and the compiled source fingerprint.

## x64 runtime

Debug, Release, and ASan Debug each passed all 19 ordinary suites with zero capability skips. All 57 suite receipts match the current profile executables, and each profile's current artifacts and build attestation are recorded in `qualification-summary.json`. The x64 ASan Debug run also detected its intentional expected-failure AddressSanitizer probe. That probe exited with the expected failure status and is separate from the 19 ordinary suites.

## ARM64 build evidence

ARM64 Debug, Release, and ASan Debug canonical cross-builds passed. The packet records all 15 build artifacts, their hashes and sizes, and ARM64 PE architecture for executables. This is cross-build evidence only; no ARM64 runtime claim is made.

## Remaining gates

Hosted run `40197354712` is still in progress at this snapshot. Its six Full native profile steps have passed, but the ARM64 ASan annotations-disabled consumer step and paired benchmark are still running; no final CI conclusion is available. This packet does not label CI green, establish quiet-host behavior, qualify the performance policy, or close the separately governed interactive/foreground gate.

All JSON content is a privacy-safe aggregate. Source identifiers are relative paths; no local paths, machine names, account details, or source text are included.
