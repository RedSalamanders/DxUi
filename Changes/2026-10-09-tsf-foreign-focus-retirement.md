- 2026-10-09: Preserve a different TSF document's focus while restoring a retired native editor's HWND association,
  including failed staged activation. Revalidate the host transition and resulting document before restoring focus,
  so a newer callback-selected document survives. Diagnose foreign-document setup separately from retirement.
