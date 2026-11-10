# Qualified SDK producer caller: local qualification

The manual SDK caller adds an explicit QualifiedAbiV2 choice, retaining LegacyV1
as the default and all three existing application source pins. The reviewed
shared controller selects the native compiler host and emits a separate portable
ABI reference only after every existing native SDK qualification gate passes.
This does not change C++ code, SDK payloads, consumer pins or application verdicts.

Local checks passed: complete validation/tooling, format, all 19 normal x64 suites
in Debug, Release and ASan Debug, the actual ASan detector, and all six builds
with zero warnings/errors. ARM64 builds are cross-compilation only; local focus
and native ARM64 runtime coverage are separate. Performance output was unpaired
and is not a controlled acceleration comparison. Exact source/profile receipts
and logs remain in the worktree .build directory, as listed in evidence.json.

The executed base was 1e8fcaac26eb099c3b1e5fa838652bf4b0447c97 with this caller/pin
change applied. The producer controller is e4e6a14c6e7d7e2c20b72331e4a2ae8d117703f5.
Documentation added after those executions is not retroactively included.

The initial concurrent x64/ARM64 dependency restore exposed an existing shared
vcpkg-tool bootstrap clone race. Its failure is preserved. After x64 drained,
the sequential ARM64 restore passed; that retry does not fix the race. S5 in the
organization WIP plan retains the bootstrap/ownership guard work. Hosted SDK
architectures use independent runner workspaces.

After integration, new explicit producer attempts must pass on both native
architectures for each unchanged source pin. The exact reviewed caller blob,
engine, run/attempt and original archive/reference/receipt must be admitted by
the separate qualified transport policy before any live consumer SDK2 hit can
be accepted. No existing source/native/package gate is removed here.
