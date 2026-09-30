# Described-menu common-scene memory optimization

Status: **ACTIVE**. Library scope; consumer adoption stays separate.
Baseline: `e47c836` (main before #24). Candidate: `6f769ab` (#24 merged), then later main.

## Goal

Remove, or explain with evidence, the clean private-memory increase that the
[described-menu waiver](../../Core/Core_PerformanceAndResources.md) accepts on the default
complex-UI fixture: at most 1,175,552 bytes (+4.6%) of clean-round median private memory in
x64 Release WARP. Then remove or tighten the waiver. Menu behavior, visuals, accessibility and
the public `secondaryText`/`accessibleName` fields stay as they are.

## What is known

- The default scene contains no menu. Outside menu popups, #24 adds a UIA control-type mapping,
  a UIA focus rule and a `SetFocusControl` flag, none of which allocates. No new static object
  allocates.
- In the September 21 phase samples, entry and device memory are equal and the candidate's
  initialized scene is 176,128 bytes smaller. The differences appear during warm-up (+253,952)
  and capture (+249,856) and reverse in the final hidden sample (-258,048).
- At dirty completion the September 23 nine-phase heap diagnostic shows -32,602 live heap bytes but
  +528,384 committed and +451,296 free heap bytes. After teardown live heap differs by -338 bytes
  while the extra capacity persists.
- For cycles 40–59 of the sixty-cycle retention diagnostic, clean private medians converge:
  34,385,920 original versus 34,357,248 candidate.
- The user's parallel source ablation on `codex/menu-description-layout` (`0f35bab`) recorded a
  same-source control (H1 to H2) that flags clean private bytes at +2.833%. Clean private memory can
  move beyond its 2% band on that machine without any source change.
- Pair three's single dirty-round increase (+2,408,448 private bytes, +8.67%) did not repeat and is
  not waived.

The leading hypothesis is allocator capacity that varies with binary layout and thread timing
rather than retained data. It is not yet shown.

## Progress

On 30 September the row-layout item was evaluated on `improve/menu-described-row-layout` and implemented (the
[row-layout packet](../../../Measurements/MenuDescriptions/2026-09-30/row-layout/README.md) keeps the evidence). The
item's premise compared one formatted layout with separate layouts, but main already shared identical labels within a
popup, which had taken the repeated-label probe to 246,338 bytes, so the question was whether one layout per row also
beats sharing. It does wherever labels differ and loses slightly where one label repeats. A described row now holds
one layout, made of the label, an empty spacer paragraph and the description, whose spacer is sized so the description
starts where a layout of its own was drawn; the description colors through one brush per popup. Main and the
candidate draw all 420 captures of a five-theme, eight-fixture, four-DPI matrix and all six gallery sheets byte for
byte alike, so the two-field spacing, colors, tab stops, color glyphs and wrapping are kept. Whole-menu live heap
(Release x64, `MenuResourceScaling` v5, open minus before): twelve, 24 and 48 rows with distinct labels use 34%, 37% and
38% less (596,882 to 395,910, 1,096,836 to 695,752 and 2,100,287 to 1,298,940 bytes), and rows that all repeat one
label use 1.6%, 4.8% and 6.9% more (388,728 to 394,952, 660,862 to 692,886 and 1,208,673 to 1,292,258), the fixtures
the sharing was built for. A layout costs about 17 KB plus about 30 bytes a character, so sharing beats a layout per
row only where a label repeats in about seven rows of one popup. The tests, ten mutants and the decision to drop the
sharing are in the packet; a hybrid that keeps main's sharing for such rows was not built, because it needs a second
drawing and measuring path for a case that only the synthetic fixtures exercise. The rise where every row repeats one
label is a measured cost that the packet leaves to the developer, with its options. This is open-menu live heap, not
the waived common-scene flag; the items below still apply.

On 27 September the first
[hosted paired set](../../../Measurements/MenuDescriptions/2026-09-27/paired-hosted/README.md) compared
`e47c836` with `6f769ab` in the Default scenario. No memory metric flags: clean private memory moves by
-0.50% and -0.70%, so the waived increase does not reproduce there. Timing flags also appear in both
same-source controls and are runner noise. One hosted set cannot remove the waiver; the quiet local
sets below decide it.

## Execution

- [ ] Re-measure on main with `performance-paired.ps1 -BaselineRevision e47c836 -CandidateRevision 6f769ab
  -Scenario Default`. The two revisions differ only by #24's library change. Run at least two
  A1/B1/B2/A2 sets on a quiet local machine and one hosted CI dispatch, labelled as hosted. Keep every
  comparison; the September 21 evidence stays as recorded. The hosted set is done (see Progress);
  the quiet local sets remain.
- [ ] Add a placebo control: `e47c836` plus an inert, unreferenced change of similar code size. If it
  reproduces a comparable clean private-memory flag, the waived difference is layout or allocator
  variation rather than a cost of the feature.
- [ ] Sample the opt-in heap diagnostic (`Tests/Support/HeapDiagnostic.h`) at each benchmark memory
  phase on both sides to separate live heap from committed and free capacity.
- [ ] If a code-attributable cost exists, remove it without changing menu behavior: defer or avoid
  the allocation, reuse storage or reduce transient peaks. Measure again with the same pairs.
- [x] Separately, evaluate one formatted layout per described row. The isolated twelve-row probe
  measured 453,754 live bytes for separate primary and secondary layouts versus 250,346 for one
  formatted layout per row. This targets the open-menu feature cost, not the waived flag, and must
  keep the two-field spacing and drawing contract. Done on 30 September and implemented (see
  [the row-layout packet](../../../Measurements/MenuDescriptions/2026-09-30/row-layout/README.md) and Progress):
  the contract and all pixels are kept, and live bytes fall by a third for every menu whose labels differ and rise up to
  7% where one label repeats in every row.
- [ ] Close out. When repeated paired runs are within the bands, remove the waiver. If the difference
  proves to be measurement variation, present that evidence for a developer decision; never change
  a threshold or baseline instead. Update the contract, the
  [described-menu plan](MenuDescriptions_2026-09-21.md) and `docs/performance.md`, then move this
  plan to Done.

## Rules

Measure the retained baseline before implementing and compare on the same fixture; unpaired results
do not establish non-regression. Hosted CI runs are serial on one machine but not a quiet desktop,
so label them. Code changes need all six native profiles, and gallery pixels must stay unchanged.
