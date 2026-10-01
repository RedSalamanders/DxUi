# Merge, toolchain and host-message hygiene

Status: **DONE** (1 October 2026). The four library items are merged, the batch's paired set establishes no regression,
and each consumer tracks its follow-ups in a plan of its own. Library scope.
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

- [x] Changelog fragments under `Changes/`, which `Tools/Fold-Changelog.ps1` folds into `CHANGELOG.md`. The plan index
  lists one plan per entry, entries separated by blank lines (`improve/changelog-fragments`, #39, merged as `edd2b27`).
- [x] vcpkg uses the Visual Studio installation the build discovers and its default toolset, as MSBuild does
  (`improve/vcpkg-default-toolset`, #40, `52b5452`).
- [x] Every DxUi private window message is registered by a name of the form
  `RedSalamanders.DxUi.<Component>.<Purpose>.v1`, so DxUi reserves no `WM_APP` value
  (`improve/registered-window-messages`, #41, `949837f`).
- [x] `GridSelectionModel` answers membership in O(log n), with a retained measurement (`improve/selection-membership`,
  #43, `e72cf62`; `Measurements/GridSelection/2026-10-01`).
- [x] A paired complex-UI set of the batch. It ran on a hosted runner, not a quiet machine: the hosted paired benchmark
  (#46) compared the base `726b43d` with `e72cf62`, main after the four items, six runs per side (workflow run
  36927511775). Default, MultilineGrid and MultilineGridDistinct are each within the noise budget, with no regressed
  metric and one improved in MultilineGrid. Single pairs flagged metrics, but their same-binary controls drifted by up to
  40%, so only the set result counts. None of the four items changes a visual, so the gallery stayed as it was.
- [x] Consumer follow-ups recorded in the WIP plans of RedSalamander, RedXe and RedPrism (see below). This plan closes
  when the four library items are merged and the follow-ups are recorded.

## Consumer follow-ups

Each consumer tracks three items in its own WIP plan:

- When it adopts the pin with registered messages, its windows forward every message, including registered ones
  (0xC000–0xFFFF), and it drops any reservation of a DxUi value.
- Its own vcpkg restore uses the discovered default toolset.
- It restores the DxUi pin with Git long paths, or as a sparse checkout without `Measurements/`.

Each plan was written as a documentation-only pull request, then merged into that consumer's adoption branch, which pins
`271bd54` (API revision 3) and implements the plan:

- RedSalamander: `DxUi_HostMessagesToolsetAndRestore_2026-10-01` (I27), from
  [DualTail/RedSalamander#109](https://github.com/DualTail/RedSalamander/pull/109), on branch
  `dxui/update-main-2026-09-30`. Its qualification task stays open until the Full suite has run on the pin.
- RedXe: `DxUiFollowUps_2026-10-01`, from [RedSalamanders/RedXe#29](https://github.com/RedSalamanders/RedXe/pull/29),
  implemented and closed in [RedSalamanders/RedXe#30](https://github.com/RedSalamanders/RedXe/pull/30).
- RedPrism: `DxUiFollowUps_2026-10-01`, which also covers its DxUi pin `c403052` (125 commits behind) and Tree
  multi-select, from [RedSalamanders/RedPrism#2](https://github.com/RedSalamanders/RedPrism/pull/2), implemented in
  [RedSalamanders/RedPrism#3](https://github.com/RedSalamanders/RedPrism/pull/3). A pin with Tree multi-select (#35, merged
  after `271bd54`) and one run of its clipboard checks remain in that plan.

## Rules

Each library item is its own branch and pull request. It needs the full `test.ps1` matrix and the three ARM64 builds,
a changelog fragment rather than a `CHANGELOG.md` edit, and its contract and docs updates. Measure before and after on
the same fixture. Never rebaseline a flagged result without the developer's decision.
