- 2026-10-09: Use native temporary paths in performance identity fixtures on Windows and Linux. Derive the
  immutable base judge's version from its actual source, allowing both legacy and versioned bases while retaining
  source-hash, receipt-byte and version-mismatch rejection checks. Correct the native edit-message fallback fixture
  to use an edit message and verify HWND caption routing through application/default handlers with and without
  logical editor focus. Preserve ARM64 foreground failures and add popup, activation and read-only hosted-desktop
  diagnostics to identify their cause without weakening focus guards or approving skips.
