# Contributing

Read [AGENTS.md](AGENTS.md) and the owning contract in [Specs](Specs/README.md).
Make a focused branch, describe the observable change and tests, and keep active plans current.
DxUI is the canonical home of the shared library. Edit its source and tests here; consumer-specific changes land
in that consumer. Record any required backport in [the migration ledger](Specs/Done/SourceImport/migration-ledger.md).
The historical source hashes document origin; they are not hashes of the current editable files.

Dependency updates require source and build fingerprints, all required configurations, relevant regression tests
and a consumer lock update. Do not assert performance without recorded evidence. For new controls, cover semantics,
keyboard, pointer cancellation, UIA, DPI and layout limits along with rendering.

Review [docs](docs/README.md) with every code change. Update affected instructions and regenerate visible changes
with `gallery.ps1 -PublishDocs`; explain when a nonvisual change needs no screenshot refresh. Capture performance
before implementation and compare afterwards using [the measurement workflow](docs/performance.md). A confirmed
regression requires developer advice with measured options before acceptance. The formatting workflow checks PRs;
its manual apply mode can commit formatting on a selected branch.

Formatting requires clang-format 22.1.3. To use the same checksum-pinned Windows x64 tool as CI from the repository root:

```powershell
python -m pip install --require-hashes --only-binary=:all: --no-deps --target .build/format -r Tools/requirements-format.txt
./format.ps1 -Check -FormatterPath .build/format/clang_format/data/bin/clang-format.exe
```

Omit `-Check` to apply formatting. `DXUI_CLANG_FORMAT` can supply the path for repeated use. The formatter is development
tooling only; library consumers do not restore it.
