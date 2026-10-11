# Retained Release-attempt1-failed focus attempt

Menu fails to find the matching visible Shadow Desktop popup before testing shadow-margin dismissal. NativeTextInput and both menu resource suites pass with zero skips. The completed lease preserves exit 1 and verifies unchanged foreground/focus and restored pointer. The slow unpaired parent report remains an observation, not a regression or proven failure cause.

This complete attempt ran on source commit 5763d57ed716429f95096eb0e1368425052ac19a, before the later setup-diagnostic
test changes. The raw suite receipts, lease result, unfiltered logs and canonical-performance.json retain their
original provenance and bytes. The parent canonical comparison is unpaired and supplies no performance acceptance.
The successful Debug attempt qualifies its own binary only; failed attempts do not become passes through later retries.

copy.receipt.txt binds the original generated outputs at capture time. Later runs legitimately replace those generated
paths. archive-copy.receipt.txt binds the separately retained external snapshot to these immutable archive copies.
Where present, post-focus.state.receipt.txt records the independently read full actual source/toolchain/SDK/dependency/
artifact state, which exactly matches the pre-focus parent state. The Debug failure was separately compared with the
following successful Debug retry's complete actual state. Provenance equality does not imply success or restored input.

The setup failure precedes the zero-size operation. Missing popup discovery does not prove that no HWND was created.
Lease completed means every child was attempted, not that every child passed. A resource child may be created/resumed
before foreground grant fails; launched=false establishes no validated foreground execution, not no process creation.
cursor.atExit is the pre-restoration pointer position. A later --check proves read availability at that later instant.
The [validation inventory](../../README.md) retains all canceled, failed and successful attempts separately.
