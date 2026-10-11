- Retain canceled, failed and successful native focus attempts separately, and add bounded activation/focus/session
  setup observations to the zero-size NativeTextInput fixture. Reset success per attempt and preserve its assertions,
  activation order, timeouts and foreground-loss policy. Documentation, specification and design-system/gallery
  review found no visible product change; published pixels remain applicable.
  Record four complete ASan foreground-suite passes with zero skips, exact before/after provenance and verified
  desktop restoration. Preserve the earlier cancellations separately; Release/Debug foreground qualification and
  original failure attribution remain open.
