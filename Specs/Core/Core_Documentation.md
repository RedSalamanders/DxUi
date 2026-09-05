# Documentation and generated gallery

Status: normative current contract
Last reviewed: 2026-09-05

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

Every catalog control MUST have a usage entry in `docs/controls.md`, a populated gallery tile and meaningful behavior
tests. Additions/removals must update these together. The generated gallery includes light, dark, rainbow light,
rainbow dark and high-contrast sheets plus the supplied-device example. Generate with `gallery.ps1 -PublishDocs`,
review all sheets for clipping/overlap/missing content, and publish PNGs, Markdown/HTML indexes and the generation
receipt in `docs/gallery`. Native tests/baselines remain distinct; never rebaseline tests merely to match a changed
documentation screenshot.

Intermediate outputs/logs stay in `.build`. Published gallery assets and explicitly reviewed independent receipts
under `Measurements/` are the deliberate exceptions. Validate all local links, catalog documentation coverage and image hashes with
`validate-specs.ps1`. A change is incomplete until required docs and gallery updates are present and checked.
