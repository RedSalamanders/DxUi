# Final x64 Debug correctness

`test.ps1 -Configuration Debug -Platform x64 -SkipBuild` passes all 18 suites against the final
disclosure-event implementation. Menu records nine foreground capability skips; all other suites
record zero. The real native UIA property-change subscriber and embedded disclosure tests pass.
Retained suite receipts include executable hashes. The accompanying performance run is unpaired.

This supplements the earlier full Debug run, which predates the final event correction. It does
not replace consumer native mixed-DPI, assistive technology or native ARM64 qualification.
