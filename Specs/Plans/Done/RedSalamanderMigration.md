# RedSalamander migration

Status: **Done for the accepted local x64 Debug/Release scope**, 2026-09-13.

RedSalamander native candidate `1232d181`, rebased onto `0dd0bd6d` and incorporating
latest master `e8a9f857`, consumes
exact DxUi `13788e95` through public imports in every DxUi-using production module.
All 27 legacy implementation files and 28 old test/project/baseline files are retired.
Product adapters and product regressions remain in RedSalamander; shared controls and
their tests remain here. The migration branch has not been merged into product master.

Local Fresh Full passes x64 Debug (2,122 passed, zero failed,
52 skipped) and Release (2,118 passed,
zero failed, 56 skipped). Each promotes 20/20 entries and
verifies all 14 actual module identities. The final portable A/B/A rehearsal passes,
including 11 packaged identities and byte-identical baseline payloads before/after.
Original failures, their follow-up and skip reasons remain in the consumer archive.

The user accepted the measured resource costs and explicitly deferred further ARM64
and ASan qualification to RedSalamander's user-owned H4 HOLD on 2026-09-13. All six
build profiles remain supported; cross-builds and skipped capabilities are not native
runtime passes. Hosted CI was not used. Publication of the tested library revision,
public exact-pin restore without the local Git rewrite and consumer merge/release
remain separate lifecycle actions.

The authoritative completed record is
`Specs/Plans/Done/DxUi_SharedLibraryAdoptionAndReleasePlan_2026-09-09.md` in RedSalamander.
Reviewed final receipts are under `Specs/TestRuns/Local-x64/DxUiAdoption/` in that
repository. Deferred platform qualification remains at
`Specs/Plans/WIP/DxUi_DeferredPlatformQualification_2026-09-13.md` there.
This is an ownership routing record, not a second execution plan.

Durable ownership and qualification rules remain in
[architecture](../../Core/Core_Architecture.md),
[consumption](../../Build/Build_ToolchainAndConsumption.md) and each consumer's contracts.
A library change never implicitly updates a consumer.

This final library closeout updates adoption status, ownership routing and documentation
only. It changes no compiled input, public API, control appearance, layout, typography,
theme or gallery fixture; existing gallery pixels remain valid. The preceding library
implementation and its visual/behavioral checks are retained in SharedLibraryReadiness.
