# Contributing

Read [AGENTS.md](AGENTS.md) and the owning contract in [Specs](Specs/README.md).
Make a focused branch, describe the observable change and tests, and keep active plans current.
Shared fixes land here; consumer-specific changes land in that consumer. During extraction, record equivalent
fixes/backports in [the migration ledger](provenance/migration-ledger.md), without rewriting the frozen reference.

Dependency updates require source and build fingerprints, all required configurations, relevant regression tests
and a consumer lock update. Do not assert performance without recorded evidence. For new controls, cover semantics,
keyboard, pointer cancellation, UIA, DPI and layout limits along with rendering.
