# RedSalamander migration

Status: HOLD — later consumer; no application changes authorized implicitly by this plan

1. Inventory every current DxUi consumer and relevant tests, themes, services, plugin ownership and packaging.
2. Select a tested standalone DxUi revision and add a source lock and library project references.
3. Adapt application-specific viewer/theme/logging/message services; do not reintroduce them into the shared library.
4. Port consumers in bounded changes; preserve window/input/accessibility and resource behavior with tests.
5. Remove the old in-tree implementation only after all consumers use the library and full application validation passes.
6. Update RedSalamander normative contracts, close the migration ledger and archive this plan.

Consumer adoption requires its own exact pin and fresh product validation. Library builds alone do not change a consumer.
