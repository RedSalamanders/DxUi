# Recent-merge production review evidence

This packet supports the [active review and remediation plan](../../Specs/Plans/WIP/ProductionReview_2026-10-05.md).
It covers 28 September through 8 October 2026: 41 first-parent landings, pull requests #28 through #68, from
`6b344566c8104dea11d3564b5b89b550db7a3f19` to `bea676a1f8418141f61cab9326b9b910e89a14b4`.
The candidate is the uncommitted library-only branch `codex/production-review-remediation-2026-10-08`.
It does not update RedXe, RedSalamander or another consumer.

## Evidence interpretation

`manifest.txt` maps short retained filenames to their original paths and SHA-256 hashes. Original paths quoted
inside raw logs remain unchanged. The review scope and live main-rule responses are retained with the initial
baseline and regression evidence. Some early negative runs already contained unrelated in-progress remediation;
they demonstrate the named failure, not qualification of a pristine whole-tree baseline. Build failures and
earlier complete profile runs were superseded as the candidate changed.

The three pre-implementation Default receipts identify the same Release WARP fixture: 1280 by 720, 96 DPI,
83 controls, 1,000 model rows, 40 frames and five rounds. The first run was much slower than the next two;
all three remain retained, and a single-run improvement is not claimed.

Callback regression maps distinguish failing and supplemental cases. Twenty-two self-replacement/retirement
cases reproduce ASan failures. Twenty-five mutable-state tests fail under per-dispatch callable copying;
ScrollPanel's mutable-state case already passes. Seven final snapshot-cleanup tests reproduce ASan use-after-free;
the blur case is supplemental. Native menu refresh independently loses mutable state. The final 58-case callback
run passes; later accessible-invoke replacement and ColorSwatch keyboard regressions are included in final suites.
The ColorSwatch assertion measures a post-retirement repaint rather than claiming an ASan memory read.

## Qualification

The final library fingerprint is `FAEF6725B5A0B5DF2C2C39A107F9FE012A5A9F8B13D6B9ABFAD2F154CE558D2E`.
`source.txt` records compiled-source identity `e2ab708c48e9de9e55c9a1bfd3601e3899a3909d09e9e95a8807f7ac59018ac0`,
including the gallery fixture lifetime correction. `matrix.txt` contains 57 successful native suite reports:
19 suites each in x64 Debug, Release and ASan Debug, with 12 explicit NewControls capability skips per profile.
The final accessibility runs are reused only with identical source, artifacts, profile, scope and environment.
The older matrix is superseded. ARM64 Debug, Release and ASan Debug builds all pass.
A cross-build establishes compilation, not native ARM64 runtime. Capability skips, pending CI and omitted
focus-taking suites are not reusable passes. `closeout-manifest.txt` records the hashes of final suite logs,
raw and scoped receipts, profile runs/builds, validators and formatting. `candidate.zip` retains exact changed
source bytes; `candidate.patch` is the reviewable tracked-file diff. New changelog content is in the archive.

All five validators and the tooling tests pass through `validate.ps1`; `format.ps1 -Check` and `git diff --check`
pass. PrePush accounting is executed after the packet is frozen, so its final log and reusable tooling receipt
stay under `.build/reports/` rather than changing their own source identity by copying them here. It explicitly
preserves capability skips and repository `NOT_EVALUATED` status.

`paired-manifest.txt` maps all 109 paired report files to their original names and hashes. `paired-summary.txt`
and `paired-gate.txt` retain the driver summary and canonical gate conclusion. The unchanged benchmark harness
was applied to both sides, with no overlay, in x64 Release on the same WARP fixture. Three ABBA repetitions give
six runs per side for Default, MultilineGrid and MultilineGridDistinct. All three sets are within the investigation
bands, with no metric flagged as regressed or improved. The canonical gate passes as **no change established**;
this is not evidence that no regression exists. Only 16, 10 and 14 of the 26 metrics respectively held inside their
bands in every same-binary control; smaller shifts in the others could not be resolved against runner drift.
Library and benchmark executable hashes were rechecked after the gallery-only test edit and still match these runs.

`gallery-generation.txt` records the six Release harness images published to `docs/gallery`; `gallery-source.txt`
adds the exact source and executable identities. All six were visually reviewed. The full five-theme gallery
passes under ASan after fixing the fixture's borrowed-model destruction order. The failed and successful logs
are both retained. These are deterministic application captures, produced without activating the desktop.

Menu, NativeTextInput, MenuResources and MenuResourceScaling require an agreed interactive desktop time.
Native ARM64 execution, real IME/assistive-technology/touch acceptance, consumer pin handoffs and private
design-system artifact republication remain open. Product/API decisions, raw mutable child-span ownership,
lazy accessibility publication and selection/focus policy remain recorded in the plan. This packet is not
blanket production approval.
