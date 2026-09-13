# RedSalamander migration

Status: ACTIVE routing — implementation and qualification are owned by RedSalamander I19.

The user explicitly authorized all I19 on September 9. The consumer migration branch is rebased onto
master `0dd0bd6d`; it uses exact DxUi `13788e9` through public imports in all production modules.
At `e7244091`, all 27 legacy implementation files and 28 old test/project/baseline files are retired
from the consumer branch. Product tests and adapters remain with the application; shared control
tests remain here. The feature branch has not been merged into RedSalamander master.

Retained consumer `ad7f7735` passes all three ARM64 cross-builds, including compiler-host
propagation and all 14 module identities per profile. Later `3a43186a` Release Full passes 19/20
entries: 2,108 cases pass, five fail and 56 skip. All five failures reproduce in isolation and
concern ViewerText snapshots incorrectly gated by `_DEBUG` in test-enabled Release. Consumer
correction `836f4f97` uses `ENABLE_TESTS`: the unchanged five cases pass, its Release root build
has zero warnings/errors, and all 14 module identities match. No library code or pin changed.

Frozen consumer `a20f6680` is running the final ARM64 build and x64 build/Full/package sequence.
The earlier portable A/B/A rehearsal passes, but final packaging follows the diagnostic correction.
The user requested local validation without hosted CI. Native ARM64 runtime, resource acceptance
and publication remain open in the product checklist; prior passes do not qualify later changes.

The authoritative checklist is `Specs/Plans/WIP/DxUi_SharedLibraryAdoptionAndReleasePlan_2026-09-09.md`
in the RedSalamander repository. This file only routes ownership; it is not a second execution plan.

1. Inventory every current DxUi consumer and relevant tests, themes, services, plugin ownership and packaging.
2. Select a tested standalone DxUi revision and add a source lock and library project references.
3. Adapt application-specific viewer/theme/logging/message services; do not reintroduce them into the shared library.
4. Port consumers in bounded changes; preserve window/input/accessibility and resource behavior with tests.
5. Qualify the final consumer tree with the legacy implementation removed after every consumer has moved.
   Do not promote the migration until required fresh application validation and resource/package gates pass.
6. Update RedSalamander normative contracts, close the migration ledger and archive this plan.

Consumer adoption requires its own exact pin and fresh product validation. Library builds alone do not change a consumer.
