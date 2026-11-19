# Contributing

Read [AGENTS.md](AGENTS.md) first, then the [specification catalog](Specs/README.md) and the domain contract that
owns the behavior you change. Shared agent skills and cross-repository guidance live in
[RedHome](https://github.com/RedSalamanders/RedHome). Every RedSalamanders repository has this same file apart
from the last section.

- Work on a focused branch and open a pull request into `main`. Describe the observable change and how you
  validated it, and complete the pull request checklist.
- Keep durable requirements in the owning domain spec, not only in a plan, a pull request or a code comment.
  Multi-step work has one indexed plan under `Specs/Plans/WIP/`.
- Add one fragment under `Changes/`, as [its README](Changes/README.md) describes. Never edit `CHANGELOG.md`.
- Native builds are accepted only with zero warnings. Pull requests run compiler-free checks; native
  qualification runs on `main`, as the [Unified CI contract](Specs/Build/Build_UnifiedCI.md) states.
- Do not claim a performance result without paired evidence on the same machine and fixture.
- Screenshots come from the application's own deterministic, non-activating capture harness, never from a
  desktop screenshot. Tests that take real focus or the clipboard run only with the agreement of the person at
  the desktop.
- Update `docs/` when a change affects what a user sees, touches, configures or is prompted about.

## In this repository

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

Format with the formatter CI uses, from the repository root:

```powershell
./format.ps1 -Check   # verify without writing
./format.ps1          # format
```

`format.ps1` takes clang-format from the pinned `.ci-engine`, which installs the checksum-pinned Windows x64 build
(22.1.3 today) under `.build/shared-format`, and covers the files CI formats. There is no other formatter lookup. The
formatter is development tooling only; library consumers do not restore it.
