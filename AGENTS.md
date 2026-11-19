# DxUi development guidance

DxUi is an independent Windows C++ library. Five applications consume it through an exact `Dependencies/DxUi.lock.json`
pin: RedTilde at API 5, and RedSalamander, RedXe, RedPrism and RedMonitor at API 4 (RedSalamander on the
`maintenance/api4` line). Each consumer's adoption, ARM64/ASan qualification and release handoff remain its own. A
library change never implicitly updates a consumer.
This file is the repository-wide instruction source. Start with [Specs/README.md](Specs/README.md) and the owning
domain contract before changing behavior. Read [README.md](README.md) for actual supported capabilities.
The repository is public and its default branch is `main`. Use focused feature branches for later changes.
`maintenance/api4` is the one maintenance branch: the API 4 line for consumers that have not moved to API 5. A
ruleset forbids deleting or force-pushing any `maintenance/*` branch. Never delete it while a consumer's
`Dependencies/DxUi.lock.json` pins a commit only it holds (`RedSalamander` does today). Land fixes on `main` first
and cherry-pick them unless they apply only to API 4, and never open a pull request from it into `main`.

## Boundaries

- Public headers live under `include/DxUi`; active implementation lives under `src`. Application-specific
  settings, AV devices, viewer interfaces, messages and themes stay in consumer adapters.
- This repository is the canonical home of DxUi. Controls and host implementation live in `src/Controls`, their
  tests and baselines in `Tests/Controls`, and supported public headers in `include/DxUi`. Edit shared code here.
  Historical origin belongs in `Specs/Done/SourceImport/source-origin.json` and Git history, never a second source tree.
- The archived `Specs/Done/SourceImport/pending-dependencies.json` is the empty final extraction inventory.
  Do not freeze editable source or reintroduce unresolved application dependencies into supported targets.
- Consumers link the single pinned DxUi.lib target. They must not enumerate this repository's `.cpp` files or pass DxUi C++
  ownership through a plugin ABI. No consumer project is changed implicitly by a library edit.
- Embedded hosting borrows the application's device/context and uses its scheduling. It owns no swap chain,
  renderer HWND, worker or periodic timer. Preparation and composition are separate contracts.
- Validate the control and embedded suites for UI changes; Foundation alone cannot establish their correctness.
- New controls require a catalog/factory entry, meaningful interaction tests, a populated gallery tile, a
  docs/controls.md entry and a design-system guideline and preview (Specs/UI/UI_DesignSystem.md), committed here.
- Application screenshots for specifications, documentation, and UI reviews must be generated through the
  application's test harness with deterministic scenarios and no desktop takeover. Reuse or extend harness
  capture support rather than using Computer Use or manual desktop screenshots. Preserve scenario/build
  provenance and label proposal mockups separately from actual application captures.
- Every code change reviews docs/gallery under Specs/Core/Core_Documentation.md; update affected usage docs and
  regenerate visual changes with gallery.ps1 -PublishDocs. docs/README.md remains linked from README.
- Samples and benchmarks use library-owned synthetic models and run without consumer repositories or services.
  Keep application adoption measurements in the application repository. Retain reviewed DxUi-only receipts under
  Measurements with scenario/source explanations; docs link to them.

## Engineering

- Use Unicode, `stdcpplatest`, `/Wall`, `/permissive-`, SDL and zero warnings, with /WX disabled. Discover Visual Studio; do not
  hard-code a developer installation. Keep x64/ARM64 Debug/Release/ASan Debug buildable.
- Use WIL for owned COM/Windows resources when those targets are extracted. Borrow synchronously and retain only
  with explicit ownership. No manual Release/Destroy of an owned resource. No exceptions through Windows/COM or
  callback boundaries. Follow [the failure policy](Specs/Core/Core_FailureHandling.md): expected refusal uses checked
  nonthrowing results; any unexpected C++ exception, including `std::bad_alloc`, requires host-owned fatal diagnostics
  and termination, not catch-and-continue or ordinary OOM recovery. Capture useful failure-site context and attempt
  the configured dump. A null check after throwing STL allocation, a bare `terminate`, or changing `noexcept` alone
  is insufficient. See the [qualification record](Measurements/FailurePolicy_2026-11-06/README.md) for the
  implementation, exact tested source and accepted x64 library scope.
  Native ARM64 execution, performance-policy qualification and each application's adoption remain separate.
- Hot-path resources and queues are bounded and reused. Composition does no heap work, layout, rasterization,
  I/O or blocking waits. Idle/hidden work blocks on events; never busy poll. Measure resource regressions.
- Measure a retained baseline before implementation and compare the candidate on the same fixture. Every test.ps1
  run reports complex-UI FPS/memory. Unpaired results do not establish non-regression. Confirmed degradation requires
  developer advice with measured optimization, scope-reduction or deferral options; never silently rebaseline.
- Preserve Unicode, focus, cancellation, IME and accessibility behavior; visual similarity alone is insufficient.
- Search existing source/spec helpers before adding a parallel utility. Keep public capability status truthful.

## Drive-root output policy

Do not create scratch, caches, evidence, copied checkouts, or other output directories directly at a drive root.
Use repository-local generated outputs or an existing approved workspace. The only permitted new drive-root
output name is `<Project>.Perf`, named after the owning project (for example `RedSalamander.Perf` or
`RedPrism.Perf`), and the user must explicitly authorize the exact path before its first creation or
initialization. A matching name, environment variable, low disk space, or authorization for another drive or
project is not authorization. Keep outputs beneath the approved root; do not invent sibling root folders. Preserve
runner-specific ownership markers and authorization. Historical cleanup inventories and preserves useful evidence
and recovery data, checks process use, repairs live junction and worktree references, and verifies preservation
before removal. Never follow junctions during cleanup or leave compatibility junctions at forbidden old drive-root
paths.

## Workflow and validation

Use one indexed WIP plan for multi-step changes. Current intended behavior belongs in domain contracts; implemented
support and remaining gates are explicit. When implementation/tests pass, update normative requirements and move
the completed plan to Done. Do not leave a completed plan under WIP or close a deferred consumer migration early.
The WIP index keeps one blank-line-separated entry per plan.

Run `validate.ps1` (the five `validate-*.ps1` validators and `Tools/tests/Invoke-ToolingTests.ps1`, reporting every
failure before it fails) and `format.ps1 -Check` for relevant changes. Repository tooling is PowerShell only, and
`test.ps1` also runs the tooling tests.
Code changes require `test.ps1` in x64 Debug, Release and ASan Debug and builds in all three ARM64 configurations. ARM64 runtime support
requires native execution. Renderer changes additionally need WARP, device-loss and recorded clean/dirty/hidden
resource evidence; bootstrap foundation tests cannot satisfy those future gates.

Never edit or commit `.build` products. Build and test tools validate output paths before recursive cleanup and
never kill independently launched applications. Git actions use the user's authorization; no force push or reset
of an unrelated checkout. Do not change repository visibility without explicit user instruction.

Application documentation screenshots use the consumer test harness. Explicitly authorized brief focus
for hover/keyboard captures uses its existing warning and interactive desktop lease, with cursor/focus
restoration; static captures remain non-activating. Never substitute desktop screenshots.

DxUi's own suites that take real focus (`Menu`, `NativeTextInput` and the two menu resource fixtures) run deliberately through
`test.ps1 -Interactive`, which refuses without an interactive desktop, asks first, warns while it runs and restores the foreground
window, keyboard focus and pointer however the run ends. Run it, or any of those suites, only when the person at the desktop has
agreed to the time; otherwise leave them out of `-Suites`.
Explicit agreement to automatic start also permits `test.ps1 -Interactive -InteractiveAutoStart`: its visible
30-second countdown defaults to Start unless cancelled. It retains desktop checks, warning and verified restoration.
Ordinary `-Interactive` still asks first and defaults to Cancel. Silence supplies no automatic-start authorization.

DxUi has no publication workflow; qualified SDK ZIPs remain internal artifacts. An explicit release-only change may use
`Tools/Release-Changelog.ps1` after all fragments are folded.

## Skills

Shared skill index: `../RedHome/AGENTS.md`; packages: `../RedHome/.agents/skills/`. If `../RedHome` is missing, clone https://github.com/RedSalamanders/RedHome.git to that path. This repository's guidance and domain specs own product behavior and validation.

Read a focused repository-local skill under `.agents/skills/` for the affected work: control-development,
embedded-rendering, win32-host, input-accessibility, or consumer-integration (their referenced files must exist).
Building, restoring and toolchain failures are the shared `native-build` skill in `RedHome` (formerly build-dxui
here), with DxUi's commands, restore and interactive-suite rules in its `references/repository-facts.md`.
Cross-repository C++/WIL, performance, and specification skills live in the sibling `RedHome` repository.

## Interactive desktop tests

A test that takes real foreground focus, the cursor or the clipboard runs only after the person at the desktop has
agreed to the time; silence never authorizes one. It warns before and while it runs, targets only windows the test
owns, and restores the foreground window, keyboard focus and pointer however it ends. Use the repository's
interactive entry point and desktop lease where it has one. Every other test and every harness capture stays hidden
and non-activating, and no scoped, CI or partial pass closes a focus-taking gate.

## Scoped testing policy

The user's accepted workflow replaces unconditional full-test iteration. While editing, run `Test-Changes.ps1 -Explain`
and then the affected default; use `-Mode PrePush` to account for the remaining local and pull-request obligations
without repeating identical ones, and explicit Full where qualification requires it. A pending CI check is never a
passed result. Reuse only exact successful evidence for the same source, pin, toolchain, binary and scenario. Native
test files use contextual `<Component>.Tests.<Subject>.h/.cpp` names, listed in the native file inventory where the
repository keeps one. Focus-taking work requires agreement to the time, and no scoped pass closes that gate. Build,
runtime and platform qualification still applies to changed behavior. [Specs/Testing/Testing_Validation.md](Specs/Testing/Testing_Validation.md) and [the test guide](Tests/README.md) own the details.

## Changes workflow

Record each change as one dated Markdown fragment under `Changes/`; follow [Changes/README.md](Changes/README.md)
and [Specs/Core/Core_Changes.md](Specs/Core/Core_Changes.md). Do not hand-edit generated `CHANGELOG.md` entries.
Keep the fragment in the change: never fold it in a feature change and never edit `CHANGELOG.md`. Fragments are
folded on `main` after merge by `Tools/Submit-ChangelogFold.ps1`, which opens a fold pull request of its own. Run
`Tools/Test-Changelog.ps1` while editing; PrePush and CI run it with `-Base`, which refuses any `CHANGELOG.md` change
that is not an exact fold of fragments already on `main` or, where the repository releases through
`Tools/Release-Changelog.ps1`, the exact generated release transition. Validation is read-only.

## Shared native build policy (2026-10-28)

Use /Wall with /WX disabled in every first-party profile. A build is accepted
only with zero warnings and zero errors; fix actionable diagnostics rather than
adding blanket suppressions. Ordinary Debug uses /ZI, ASan Debug uses /Zi.
Release explicitly enables /OPT:ICF; Debug/ASan explicitly disable it.
Follow Specs/Build/Build_ConsoleAndPaths.md for the common green result, safe
terminal launch links and short dependency folders with full identity checking.

## Unified CI policy

Use [Specs/Build/Build_UnifiedCI.md](Specs/Build/Build_UnifiedCI.md) for hosted CI. PRs run compiler-free checks and formatting feedback. Main native qualification starts after five continuous quiet minutes; both native Release test branches must pass before either production Release/ZIP branch starts. Scheduled dependency/security/status checks remain compiler-free. Formatting commits, extended qualification and any ZIP or winget publication are explicit manual actions. PR checks do not satisfy native acceptance or waive applicable local scoped tests. Shared-engine pins in .ci/project.json and workflow callers must agree. Preserve dependency pins, native toolchain, evidence and desktop ownership contracts.
