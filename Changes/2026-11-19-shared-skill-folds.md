- Retired the repository-local `build-dxui` agent skill, now folded into the shared `native-build` skill in `RedHome`
  (convergence CV-2-12). Every rule and code line of it is in the shared skill, with DxUi's commands, the toolset
  pinned restore, the single-test filter and watchdog, and the interactive-suite rules in its
  `references/repository-facts.md`. Claude Code (the `redhome` plugin), Codex, and Copilot list the shared skills
  from this repository. `AGENTS.md` points at it. No build, test or product input changes.
