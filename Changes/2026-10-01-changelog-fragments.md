- Changes no longer conflict over the changelog or the plan index. A change records its changelog entry as one file under
  `Changes/` (`<yyyy-mm-dd>-<topic>.md`, one bullet; `Changes/README.md`), and `Tools/Fold-Changelog.ps1` folds the
  fragments into `CHANGELOG.md`, newest first. `validate-specs.ps1` checks each fragment's name and shape, and
  `Test-Docs.ps1` covers the fold. The WIP plan index lists one plan per entry, separated by blank lines, so updates to
  different plans merge cleanly. On 30 September five branches each prepended an entry or edited a neighbouring index
  row, so they merged one at a time and ran CI again after each re-merge.
