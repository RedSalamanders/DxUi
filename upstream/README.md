# Frozen extraction reference

These files preserve the clean RedSalamander source/test snapshot recorded in
[source-import.json](../provenance/source-import.json). They intentionally retain original namespaces, includes,
project paths and test dependencies. They are not compiled by the new library.

Do not edit this snapshot. Extract reviewed source into active `src`/`include`/`Tests`, record it in the migration
ledger, remove application coupling and migrate the relevant tests. This reference exists only for the bounded
transition; its retention/removal is reviewed at extraction closeout.
