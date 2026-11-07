# Native qualification verifier and failure evidence

The [focused verification](focused-verification.json) binds the verifier, original
EXE/PDB pairs, PDB source checksums, child evidence and tooling tests to `6b2214df`.
The complete Foundation fixture passes on native x64 Release; ARM64 Release builds
with zero warnings and errors. Both link retained SDK libraries whose 281 runtime,
public-header and dependency-policy inputs are byte-identical. This is focused
test-verifier verification, not a new six-profile SDK qualification.

All original wrappers and canonical payload members of the three failed hosted
runs at `054766f6` are independently verified. Grid's named fatal origins pass in
all three ARM64 configurations. Their separate remaining failures are preserved:
Debug's Menu cursor fixture is covered by a hosted console window; ASan completes
the native suites but rejects two external-consumer build warnings; Release loses
three named allocation origins during saved-context unwind. The old collector did
not retain the consumer build log, so those two warnings remain unattributed.

Original Release images, matching PDBs and independent live-capture symbols contain
the named YAML and retired-text origins. The default DbgHelp walk stops before
reaching them. The ARM64 verifier now tries the documented frame-pointer walk from
untouched saved-context copies after the default walk fails. Each attempt remains
bounded to 256 frames and must satisfy the same thread, IP, image, symbol and named
origin checks. Wrong-thread, wrong-IP and absent-origin controls still reject.
Native ARM64 acceptance of the fallback requires a fresh hosted run; it is not
established by this cross-build or attributed to a proven operating-system defect.

Build failures now print the first 16 actual diagnostic records without changing
the zero-warning acceptance rule. External-consumer logs go directly into retained
reports. A failed generated consumer also retains its bounded, member-verified
sources, projects, images and dumps; cache/checkouts are excluded and reparse
points, occupied outputs and exceeded budgets refuse collection. The original
qualification error remains primary if evidence collection also fails.

The lasting contracts belong to [validation](../../Specs/Testing/Testing_Validation.md)
and [build console output](../../Specs/Build/Build_ConsoleAndPaths.md). Fresh full
native ARM64, external-consumer and combined test-before-production acceptance
remain in the indexed WIP plan.
