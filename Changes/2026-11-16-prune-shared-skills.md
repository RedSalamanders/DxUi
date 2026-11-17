- Consolidated the shared `modern-cpp-wil`, `performance-resources`, and `spec-workflow` agent skills into the
  organization `RedHome` repository, retaining the six DxUi-specific skills under `.agents/skills/` and validating
  with `validate.ps1` and `Tools/Test-Changelog.ps1` (documentation/tooling only; native qualification unchanged).
  Shared-skill discovery names `../RedHome/AGENTS.md`, the package directory, and the clone URL while preserving
  local product authority.
  Skill/spec/dependency/port/build-matrix validators pass; the affected tooling gate remains blocked by existing
  workflow engine pins on `main` that diverge from `.ci/project.json`.
