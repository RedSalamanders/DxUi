# DxUi development guidance

DxUi is an independent Windows C++ library. Its first application consumer is RedXe; RedSalamander migration is later.
This file is the repository-wide instruction source. Start with [Specs/README.md](Specs/README.md) and the owning
domain contract before changing behavior. Read [README.md](README.md) for actual supported capabilities.
The repository is private and its default branch is `main`. Use focused feature branches for later changes.

## Boundaries

- Public headers live under `include/DxUi`; active implementation lives under `src`. Application-specific
  settings, AV devices, viewer interfaces, messages and themes stay in consumer adapters.
- `upstream/RedSalamander` is immutable import evidence, not a source folder to compile or edit. Extract a reviewed
  slice into active source, preserve attribution, update the extraction ledger and migrate its generic tests.
- Consumers link pinned static targets. They must not enumerate this repository's `.cpp` files or pass DxUi C++
  ownership through a plugin ABI. No consumer project is changed implicitly by a library edit.
- Embedded hosting borrows the application's device/context and uses its scheduling. It owns no swap chain,
  renderer HWND, worker or periodic timer. Preparation and composition are separate contracts.
- The current foundation target is not proof that controls, Win32 services or embedded hosting have been ported.

## Engineering

- Use Unicode, `stdcpplatest`, `/W4`, `/permissive-`, SDL and warnings as errors. Discover Visual Studio; do not
  hard-code a developer installation. Keep x64/ARM64 Debug/Release buildable.
- Use WIL for owned COM/Windows resources when those targets are extracted. Borrow synchronously and retain only
  with explicit ownership. No manual Release/Destroy of an owned resource. No exceptions through Windows/COM or
  callback boundaries; catch named exceptions only when needed, with a documented fallback.
- Hot-path resources and queues are bounded and reused. Composition does no heap work, layout, rasterization,
  I/O or blocking waits. Idle/hidden work blocks on events; never busy poll. Measure resource regressions.
- Preserve Unicode, focus, cancellation, IME and accessibility behavior; visual similarity alone is insufficient.
- Search existing source/spec helpers before adding a parallel utility. Keep public capability status truthful.

## Workflow and validation

Use one indexed WIP plan for multi-step changes. Current intended behavior belongs in domain contracts; implemented
support and remaining gates are explicit. When implementation/tests pass, update normative requirements and move
the completed plan to Done. Do not leave a completed plan under WIP or close a deferred consumer migration early.

Run `validate-skills.ps1`, `validate-specs.ps1`, `validate-dependencies.ps1` and `format.ps1 -Check` for relevant changes.
Code changes require `test.ps1` in x64 Debug and Release and builds in ARM64 Debug and Release. ARM64 runtime support
requires native execution. Renderer changes additionally need WARP, device-loss and recorded clean/dirty/hidden
resource evidence; bootstrap foundation tests cannot satisfy those future gates.

Never edit or commit `.build` products. Build and test tools validate output paths before recursive cleanup and
never kill independently launched applications. Git actions use the user's authorization; no force push or reset
of an unrelated checkout. Do not change repository visibility without explicit user instruction.

## Skills

Read a focused skill under `.agents/skills/` for the affected work: build-dxui, control-development,
embedded-rendering, win32-host, input-accessibility, performance-resources, modern-cpp-wil,
spec-workflow, or consumer-integration. These are repository-local skills and their referenced files must exist.
