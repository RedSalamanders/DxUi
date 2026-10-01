# Documentation and generated gallery

Status: normative current contract
Last reviewed: 2026-09-30

`docs/README.md` is the user entrypoint and MUST be linked from the root README. Documentation covers prerequisites,
exact-pin consumption, both hosting modes, ownership, input, layout/DPI, themes, recovery, every public control,
performance measurements and testing. Public examples must use supported headers and accurate API names.
Clearly distinguish implemented library behavior from pending consumer integration and manual validation.
Samples MUST run independently of application repositories, settings and services, using library-owned synthetic
models. Application-specific measurement archives belong in their application's repository. DxUi docs describe
its independent workloads and link to reviewed library receipts under `Measurements/`; never present consumer
integration evidence as standalone library acceptance. Scenario inspiration does not create a runtime dependency.

Every code change MUST review affected docs and gallery. Update docs in the same change whenever public API,
observable behavior, defaults, requirements, build/consumption or performance/test workflow changes. Update and
regenerate the gallery when visuals, controls, layout, typography, themes, states or examples change. A nonvisual
internal change may leave gallery pixels unchanged, but its change/plan must state why docs/gallery need no update.
Do not accept stale instructions or screenshots as complete work.

A change records its changelog entry as one fragment under [`Changes/`](../../Changes/README.md), never as an edit of
`CHANGELOG.md`, so changes merge in any order. `Tools/Fold-Changelog.ps1` folds the fragments into `CHANGELOG.md`,
newest first, in a change of its own. The active plan index lists one plan per entry, entries separated by blank
lines, so updates to different plans merge without conflicts.

Every catalog control MUST have a usage entry in `docs/controls.md`, a populated gallery tile, meaningful behavior
tests and a design-system guideline and preview under `Specs/DesignSystem/components/<Control>/`
([contract](../UI/UI_DesignSystem.md)). A new control is incomplete until its documentation, gallery tile and
design-system preview exist and the design system is republished. Additions/removals must update these together;
visual changes to existing controls update the affected design-system tokens and previews in the same change. The generated gallery includes light, dark, rainbow light,
rainbow dark and high-contrast sheets plus the supplied-device example. Generate with `gallery.ps1 -PublishDocs`,
review all sheets for clipping/overlap/missing content, and publish PNGs, Markdown/HTML indexes and the generation
receipt in `docs/gallery`. The harness sizes its offscreen capture window from the sheet rather than the desktop, so
a small display cannot crop it. CI's x64 Release job uploads the same output as `docs-gallery-x64-Release`.
Native tests/baselines remain distinct; never rebaseline tests merely to match a changed
documentation screenshot.

Publishing the gallery after a merge is not a manual copy of that artifact. The manual `Publish docs gallery` workflow
(`.github/workflows/gallery.yml`) restores dependencies, regenerates `docs/gallery` from a native x64 Release build with
`gallery.ps1 -PublishDocs`, validates the specifications and gallery, and commits the result to the branch it was run
on. It runs only on `workflow_dispatch` with an explicit boolean input and only for a branch ref, never on a push or
`pull_request_target`; it holds `contents: write` for that one job alone; it pushes with an ordinary push, never
forced, so a branch that moved since the checkout fails the run to be repeated; and it commits nothing when the
gallery is current. `generation.json` records the commit the sheets were generated from and so changes with every
commit: it is not itself a change, and a run commits only when a sheet, the HTML index or the README differs
(`Tools/Commit-Gallery.ps1`, tested against fixture repositories). A commit pushed with the workflow token starts no
other workflow, so validation runs again on the next user push. The generated sheets are reviewed in that commit's
diff like any generated change, and the review above (every sheet, for clipping, overlap and missing content) still
applies.

Intermediate outputs/logs stay in `.build`. Published gallery assets and explicitly reviewed independent receipts
under `Measurements/` are the deliberate exceptions. Validate all local links, catalog documentation coverage and image hashes with
`validate-specs.ps1`. A change is incomplete until required docs and gallery updates are present and checked.

Gallery fixtures must bound message settling by elapsed time as well as message count. Animated
full-control sheets can continuously refill the queue under sanitizer instrumentation. An explicit
synchronous redraw establishes each captured frame after settling; waiting for an empty queue is
not a prerequisite for a snapshot. This does not change the production host scheduling contract.
