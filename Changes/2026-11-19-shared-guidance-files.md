- Agent guidance now matches the other RedSalamanders repositories (`RedHome` convergence step CV-6-02). There is
  one identical `.github/copilot-instructions.md` pointer to `AGENTS.md`, and the shared-skill pointer sits directly
  under `## Skills`. The drive-root, scoped-testing, Changes, native build and Unified CI sections use the text every
  repository shares. A new shared "Interactive desktop tests" section states the focus rule, while DxUi's own
  `test.ps1 -Interactive` and release facts move into "Workflow and validation". The introduction names all five
  consumers and their API revisions, and `consumer-integration` no longer says API revision 5 only. The common
  `.gitignore` block ignores `.claude/settings.local.json` and `.claude/scheduled_tasks.lock`, and `capabilities.json`
  gains `product` and `status`. The consumer interface is unchanged, and nothing compiles differently.
