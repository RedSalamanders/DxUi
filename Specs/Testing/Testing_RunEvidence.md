# Generated test-run evidence

Status: normative current tooling contract
Last reviewed: 2026-10-21

Filesystem preservation regressions require Windows and run in the Windows tooling job. The Linux validator
reports this coverage as `WINDOWS_NOT_RUN`; its portable validation is not Windows filesystem qualification.

After argument, desktop and build preflight, native test execution writes each invocation under `.build/evidence/test-runs/<UTC-pid-guid>/payload`.
It creates a new directory and exclusive owner lease; it never reuses an old run directory or sweeps older runs.
A flushed, create-only `started.json` records the owner process and start time plus the requested profile/scopes.
The `completed.json` record is staged, flushed, independently read back and renamed without overwrite only after
payload verification. A failed or interrupted invocation retains its output. Forced termination can leave only
`started.json` and partial data; absence of a valid completion record is never a successful result.

Verification includes hidden files, long Unicode paths, ordinary file contents and named data streams (SHA-256),
128-bit file identities, attributes, creation/write timestamps and owner/group/DACL metadata. Existing reparse
ancestors and payload links are refused without traversing or deleting their targets. Directory identity is freshly
checked; sharing flags alone do not prevent all ReFS directory renames. Files still open for writing fail verification.
Unique evidence stays in place; no archive payload copy, attribute normalization or read-only clearing occurs.
Shared build/dependency/qualification files remain outside the writable run, and their existing read-only rules apply.

The record establishes retention and the outcome of the selected invocation only. It does not replace exact source,
binary/dependency/profile/scenario attestation, qualify omitted tests, or turn a capability skip, pending CI check,
cross-build, synthetic probe or failing comparison into native/product qualification. Sandbox markers, path containment,
process ownership, watchdog, desktop consent and ReFS capability checks remain required by their owning contracts.

Generated evidence stays ignored by Git. Normal builds do not sweep it. CI collects the new evidence path, including
hidden files, where the branch has an enabled native artifact workflow. Transport retention and stream support are
separate from lossless local evidence. Historical cleanup requires a separate inventory, liveness check and verified
same-volume move or full independent copy verification; a timestamp or dead PID alone never authorizes deletion.
DxUi additionally leases the fixed native `.build/test-artifacts` path for the invocation. It refuses independently
running test processes and retains pre-existing captures separately. Before/after SHA-256, streams, identity and
metadata verify each same-volume rename into the evidence payload. It preserves raw execution-path provenance.
Suite JSON compatibility views remain under `.build/reports`; the original receipt and unique log remain in the run.
A superseded compatibility view is moved, with streams intact, before publication of the next view. Interactive,
hosted, filtered and instrumented suffixes remain distinct. Performance comparison receipts are run-specific.
This changes tooling and artifact storage only; library/gallery pixels, consumer pins and product UI are unchanged.
