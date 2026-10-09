# Six-profile builds after the IMM fixture correction

All six canonical `build.ps1` invocations passed: x64 and ARM64 in Debug, Release and ASan Debug. The final
fixture sends START before its IMM preview and explicitly verifies HWND keyboard focus and the native session.
No production input guard was weakened. The complete `validate.ps1` runner and clang-format 22.1.3 check passed.

The parent and canonical MSBuild logs are retained here. `verification-manifest.txt` records the independently
checked current compiled-source, dependency/build-input and artifact identities, plus every log hash. The same
manifest is retained with its original bytes in `build-qualification-45.zip`, SHA-256
`4F560DB71C1ADE1D94C01763BA485950E48FECB05556E7717E2536E81141251B`.

This is a post-build inventory, not a reusable scoped receipt: direct `build.ps1` does not publish scoped receipts.
The six build invocations completed successfully before verification; only documentation changed between them
and this inventory. Builds establish compilation, not native runtime, consumer or paired performance qualification.
The [earlier 57 runtime receipts](../NativeQualification42/README.md) qualify their recorded source only.

Debug43's full Menu run passed with zero skips, while NativeTextInput stopped at the corrected fixture. Debug44
and Debug45 confirmations timed out before any child started; their foreground, focus and pointer were unchanged.
The current NativeTextInput desktop gate remains open. Exact-source CI, native ARM64 runtime, consumer fixtures,
Release paired policy and physical IME/assistive-technology/touch acceptance remain separate obligations.
