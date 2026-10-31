# Documentation and generated gallery

Status: normative current contract
Last reviewed: 2026-10-21

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

The [Changes workflow](Core_Changes.md) requires one fragment in each change and never a `CHANGELOG.md` edit;
fragments are folded on `main` after merge. The active plan index lists one plan per entry,
entries separated by blank lines, so updates to different plans merge without conflicts.

Every catalog control MUST have a usage entry in `docs/controls.md`, a populated gallery tile, meaningful behavior
tests and a design-system guideline and preview under `Specs/DesignSystem/components/<Control>/`
([contract](../UI/UI_DesignSystem.md)). A new control is incomplete until its documentation, gallery tile and
design-system preview exist and are committed with the change. Additions/removals must update these together;
visual changes to existing controls update the affected design-system tokens and previews in the same change. The generated gallery includes light, dark, rainbow light,
rainbow dark and high-contrast sheets plus the supplied-device example. Generate with `gallery.ps1 -PublishDocs`,
review all sheets for clipping/overlap/missing content, and publish PNGs, Markdown/HTML indexes and the generation
receipt in `docs/gallery`. The harness sizes its offscreen capture window from the sheet rather than the desktop, so
a small display cannot crop it. CI's x64 Release job uploads the same output as `docs-gallery-x64-Release`.
Native tests/baselines remain distinct; never rebaseline tests merely to match a changed
documentation screenshot.

DxUi has no hosted publication workflow, for either SDK releases or the documentation gallery. Generate reviewed
documentation locally with `gallery.ps1 -PublishDocs`, run the specification/gallery validators, and include changed
assets in an ordinary pull request. CI still executes the native gallery as regression coverage and retains internal
artifacts; it never publishes or commits them. The review above (every sheet, for clipping, overlap and missing
content) still applies. `generation.json` records the source of generated sheets; provenance-only changes never
replace the required visual review.

Intermediate outputs/logs stay in `.build`. Published gallery assets and explicitly reviewed independent receipts
under `Measurements/` are the deliberate exceptions. Validate all local links, catalog documentation coverage and image hashes with
`validate-specs.ps1`. A change is incomplete until required docs and gallery updates are present and checked.

Each reviewed benchmark/profiling JSON receipt must carry its real DxUi workload owner, fixture-input hashes and a neighboring
scenario explanation. Raw CI/status/child reports and private machine/foreground diagnostics stay in generated
output or an explicitly authorized private archive. A summary-only packet may retain an original-path, size and
SHA-256 inventory; preserve and verify the raw bytes before removing redundant copies. Do not fabricate benchmark
metadata or rewrite historical results to satisfy the receipt schema. Evidence intended for checkout obeys the
[consumer path-length limit](../Build/Build_ToolchainAndConsumption.md).

Hash-bound packets preserve their declared file bytes across Git checkouts, using an explicitly scoped root or
packet-local `.gitattributes` rule. Verify staged blob bytes against the public manifest before publishing. Source
reconstruction from Git requires the committed effective checkout attributes and an exact revision/tree binding;
label reconstructed checkout bytes separately from a retained original source archive. Do not guess line endings
to turn a mismatched source hash into a verification pass.

Gallery fixtures must bound message settling by elapsed time as well as message count. Animated
full-control sheets can continuously refill the queue under sanitizer instrumentation. An explicit
synchronous redraw establishes each captured frame after settling; waiting for an empty queue is
not a prerequisite for a snapshot. This does not change the production host scheduling contract.
