# Contributing

Read [AGENTS.md](AGENTS.md) and the owning contract in [Specs](Specs/README.md).
Make a focused branch, describe the observable change and tests, and keep active plans current.
DxUI is the canonical home of the shared library. Edit its source and tests here; consumer-specific changes land
in that consumer. Record any required backport in [the migration ledger](provenance/migration-ledger.md).
The historical source hashes document origin; they are not hashes of the current editable files.

Dependency updates require source and build fingerprints, all required configurations, relevant regression tests
and a consumer lock update. Do not assert performance without recorded evidence. For new controls, cover semantics,
keyboard, pointer cancellation, UIA, DPI and layout limits along with rendering.
