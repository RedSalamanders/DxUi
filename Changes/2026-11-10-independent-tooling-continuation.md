- Retain independently passed scoped tooling before a later native failure, so an
  identical continuation does not repeat completed validators. Keep complete input
  checks, failed-run revocation, instrumentation isolation and native/foreground
  obligations; no failed native batch becomes a pass.
