- Named the API 4 maintenance branch in `AGENTS.md`. The branch formerly called `ej/api4-maintenance-4cdf1ce5` is now
  `maintenance/api4` on GitHub, and a ruleset forbids deleting or force-pushing any `maintenance/*` branch. It stays
  for consumers that still lock API 4; `RedSalamander`'s `DxUi` lock pins a commit that only this branch holds.
  Consumers clone the whole repository and check out the locked commit, so the rename changes no restore, build or
  test input.
