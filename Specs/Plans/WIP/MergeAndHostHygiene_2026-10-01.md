# Merge, toolchain and host-message hygiene

Status: **ACTIVE**. Library scope, with consumer follow-ups tracked in RedSalamander, RedXe and RedPrism.
Base: main `726b43d`.

## Why

The 30 September batch showed four avoidable costs:

- **Serial merges.** Every branch prepended its changelog entry and edited a neighbouring row of the plan index's
  table. Five branches conflicted with each other, merged one at a time and ran CI again after each re-merge.
- **Mixed toolsets.** vcpkg builds dependencies with the newest MSVC toolset of the Visual Studio installation, while
  MSBuild uses its default toolset. On a machine whose newest toolset lacks the x64-hosted ARM64 compiler (14.52 here),
  every fresh ARM64 restore failed. A consumer that links compiled vcpkg libraries also risks a library from a newer
  toolset than its linker.
- **Colliding window messages.** DxUi's private window messages are `WM_APP` offsets, five of them on the
  application's own window. RedSalamander's test message `WM_APP + 0x6A` is DxUi's accessibility action, and consumers
  had to reserve DxUi's values.
- **Linear selection lookups.** `GridSelectionModel::IsSelected` scans the whole selection, once per painted row.
  After Ctrl+A on a large list, each paint costs the visible rows times the selection size.

## Execution

- [ ] Changelog fragments under `Changes/`, which `Tools/Fold-Changelog.ps1` folds into `CHANGELOG.md`. The plan index
  lists one plan per entry, entries separated by blank lines (`improve/changelog-fragments`).
- [ ] vcpkg uses the Visual Studio installation the build discovers and its default toolset, as MSBuild does
  (`improve/vcpkg-default-toolset`).
- [ ] Every DxUi private window message is registered by a name of the form
  `RedSalamanders.DxUi.<Component>.<Purpose>.v1`, so DxUi reserves no `WM_APP` value
  (`improve/registered-window-messages`).
- [ ] `GridSelectionModel` answers membership in O(log n), with a retained measurement (`improve/selection-membership`).
- [ ] A paired complex-UI set of the batch on a quiet machine. The gallery stays unchanged or is regenerated.
- [ ] Consumer follow-ups recorded in the WIP plans of RedSalamander, RedXe and RedPrism (see below). This plan closes
  when the four library items are merged and the follow-ups are recorded.

## Consumer follow-ups

Each consumer tracks three items in its own WIP plan:

- When it adopts the pin with registered messages, its windows forward every message, including registered ones
  (0xC000–0xFFFF), and it drops any reservation of a DxUi value.
- Its own vcpkg restore uses the discovered default toolset.
- It restores the DxUi pin with Git long paths, or as a sparse checkout without `Measurements/`.

## Rules

Each library item is its own branch and pull request. It needs the full `test.ps1` matrix and the three ARM64 builds,
a changelog fragment rather than a `CHANGELOG.md` edit, and its contract and docs updates. Measure before and after on
the same fixture. Never rebaseline a flagged result without the developer's decision.
