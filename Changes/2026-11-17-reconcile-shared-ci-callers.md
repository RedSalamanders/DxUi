- Reconciled the common workflow callers with the existing shared engine pin in `.ci/project.json` through
  the pinned engine's canonical generator, removing partial dependency-update drift without selecting a new
  engine version. Compiler-free CI configuration, workflow, scoped-planner and affected tooling checks cover
  the repair; native, interactive and platform qualification remain separate.
