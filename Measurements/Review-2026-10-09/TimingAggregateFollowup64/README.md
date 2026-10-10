# Timing aggregate validation

This DxUi-owned synthetic malformed-receipt packet records a lead review finding in newly added timing diagnostics
at `7f2e9595a26f8a84cd11f9b769abb16e70d9716f`. Forty individually finite `1e308` frame samples overflow their sum;
the old validator accepted a falsely tiny positive FPS through rounding tolerance near zero. The original executable
PowerShell reproduction is retained as a validation failure, not a performance result.

The tooling-only repair requires a finite positive frame total and a positive integer QPC frequency within its
Int64 carrier. Meaningful regression cases and an independent witness reject the overflowing sum, an exact Int64+1
frequency and a fractional frequency while accepting a valid complete receipt. The affected Tooling scope passes
all five validators, all tooling cases and Windows ASan runtime staging/rejection checks; formatting passes.
Compiled native source and the workload are unchanged. The renewed normalized judge review target is
`EB1B3047ED8B031383AEBA4D9A72A37D3719B1320A46381EFF042897CCB72E33`; the policy remains explicitly unqualified.
No judged metric, band, inference or exact budget changes, and no candidate performance pass is established.

`timing-aggregate-validation.zip` retains the original reproduction, source snapshots, raw logs/receipt and the
independent auditor. Each entry is reopened against original length/SHA-256; `archive-manifest.txt` records those
identities. Hosted native execution of the frozen candidate, final-source tooling/paired qualification, trusted
policy seeding and physical adoption remain separate obligations.
