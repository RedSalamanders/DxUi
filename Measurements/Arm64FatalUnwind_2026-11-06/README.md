# ARM64 fatal-origin verification and preflight evidence

Implementation: `3a5ae6b0ee109118572a3358f974a3ee41f264e5`, API 5.

[Qualification](qualification.json) independently verifies the original 681-file input snapshot, six
zero-warning builds, all 66 noninteractive x64 suite executions and the real ASan detector. Each retained
native/run archive member is checked. Every member of the streamed failure-image archives is also checked
against the original linked EXE/PDB inventory. Four completed profiles and the remaining two ARM64 profiles
have separate original progress ledgers and a verified continuation index. The intervening output-process
inspection refusal remains retained without an assigned historical cause.

The ARM64 verifier uses dynamically discovered `StackWalk2` with the target-attribute callback required for
pointer authentication. Its live-target PAC mask applies only to a dump from the same native machine and
original held child-process handle. Saved context/thread, native architecture, PE/PDB identity and the named
failure origin remain required. A missing supported walker or incompatible dump refuses verification.
See Microsoft's [StackWalk2 contract](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk2)
and [target attributes](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nc-dbghelp-pget_target_attribute_value64).

Fifty-eight evidence fixtures cover previous/current failures, exact profile and path ownership, redirected
paths, an independently named live executable, compressed EXE/PDB retention and every member's original
length/hash. Streaming avoids uncompressed PDB staging. The original disk-full finalization and earlier
failed hosted cohorts remain retained separately.

The [failure policy](../../Specs/Core/Core_FailureHandling.md) and
[validation contract](../../Specs/Testing/Testing_Validation.md) own the lasting behavior. The
[scoped closeout](../../Specs/Plans/Done/Arm64FatalUnwind_2026-11-06.md) records local implementation
acceptance. Native ARM64 execution, the combined hosted production gate, physical input/AT and performance
remain in the [active plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
