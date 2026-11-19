- Converged the remaining root files with every RedSalamanders repository (convergence CV-6-03, B-12 default):
  `CONTRIBUTING.md` is the common text with all of its DxUi rules kept in a repository section, and the common pull
  request template and `.vsconfig` are added. CODEOWNERS names the workflows and Dependabot on their own lines, as
  every repository's does; no path changes owner. Validated with `Tools/tests/Test-CodeOwners.ps1`,
  `validate-specs.ps1` and `Tools/Test-Changelog.ps1`.
