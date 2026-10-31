# Contributing

Read [AGENTS.md](AGENTS.md) and the owning contract in [Specs](Specs/README.md).
Make a focused branch, describe the observable change and tests, and keep active plans current.
DxUi is the canonical home of the shared library. Edit its source and tests here; consumer-specific changes land
in that consumer. Record any required backport in [the migration ledger](Specs/Done/SourceImport/migration-ledger.md).
The historical source hashes document origin; they are not hashes of the current editable files.

Dependency updates require source and build fingerprints, all required configurations, relevant regression tests
and a consumer lock update. Do not assert performance without recorded evidence. For new controls, cover semantics,
keyboard, pointer cancellation, UIA, DPI and layout limits along with rendering.

Review [docs](docs/README.md) with every code change. Update affected instructions and regenerate visible changes
with `gallery.ps1 -PublishDocs`; explain when a nonvisual change needs no screenshot refresh. Capture performance
before implementation and compare afterwards using [the measurement workflow](docs/performance.md). A confirmed
regression requires developer advice with measured options before acceptance. Required paired benchmarks remain explicit
manual qualification; a confirmed degradation or inconclusive result does not pass that gate. PRs run compiler-free
checks and formatting feedback. Meaningful main inputs qualify after five quiet minutes, with both native x64/ARM64
Release regressions required before production SDK ZIPs. The explicit formatting action may commit a reviewed PR's
formatting and request fresh checks. Generate changed gallery documentation locally, review every sheet and include
it in an ordinary PR. DxUi has no hosted publication workflow. `.github/CODEOWNERS` names the owner
of the workflows, `Tools`, the performance scripts and the agent instructions.

Formatting requires clang-format 22.1.3. To use the same checksum-pinned Windows x64 tool as CI from the repository root:

```powershell
./Tools/Install-ClangFormat.ps1
./format.ps1 -Check
```

The installer downloads the pinned wheel, checks its SHA-256 and extracts only `clang-format.exe` under `.build/format`,
where `format.ps1` looks first. Omit `-Check` to apply formatting. `-FormatterPath` or `DXUI_CLANG_FORMAT` can name
another clang-format 22.1.3. The formatter is development tooling only; library consumers do not restore it.
