- Every local skill description now says what the skill does and then when to use it ("Use when ..."), the form
  the shared `RedHome` skills already use, because an agent picks a skill from its description alone (`RedHome`
  convergence step CV-2-13). `consumer-integration`, `control-development`, `embedded-rendering`,
  `input-accessibility` and `win32-host` gain the clause, taken from each skill's own body; `build-dxui` already
  had one. Only the five front-matter descriptions change. `validate.ps1` passes; no build, test or product input changes.
