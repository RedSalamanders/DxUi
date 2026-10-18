# Closeout checks

The initial packet replay, two one-byte negative copies and post-closeout five
validators/formatter/scope checks are preserved here. All checks pass; negative
copies fail specifically at the altered native receipt/source hash and leave
those original targets unchanged. These review records are included in the final
seal after the initial replay. Staged Git-only replay is retained separately with
the private original binaries, avoiding a self-referential tree-hash record.

See [the component record](../README.md) for the exact native/build/resource scope
and still-open full-engine, physical/platform and frame-performance gates.
