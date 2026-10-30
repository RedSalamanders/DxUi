- Refresh the production review of the ten-day merge window through #137 at `09f6d96a`. Every earlier P1 still
  reproduces. Second reviewers confirmed, corrected or refuted each earlier unverified item. The refresh adds a
  `Grid::SetModel` dangling model pointer after allocation failure, typed menu characters that cancel pending
  pointer cascade timers, tree-walk, hit-test and syntax-adoption costs, gaps in the zero-warning gate and the
  changelog fold, simplification proposals and seven new developer questions. Review only; no library, test or
  tooling behavior changes.
