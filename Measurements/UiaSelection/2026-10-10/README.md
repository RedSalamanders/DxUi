# Bounded UIA selection-event investigation

The archived ASan Debug failure heard an extra `Selected Ligne 3` notification while republishing an unchanged
Grid selection. Its [original log](../../Layout/2026-10-10/first-Accessibility-ASanDebug.log) and same-binary passing
isolation remain unchanged. The original cause is unproven.

This follow-up preserves the expected event payloads, per-step deduplication and 500 ms quiet interval. It fixes
the client helper's discarded quiescence timeout and records a bounded callback-entry/completion timeline plus
provider raise-attempt timeline for the exact Grid test. An unchanged republish must make zero provider attempts
and yield no client-observed selection events. Provider attempts are not delivery success or payload identity.
The assertion uses the same frozen event cutoff as its diagnostics, before a failure's tree query pumps messages.

`Invoke-Reproduction.ps1` declares ten isolated ASan Debug processes, then one process with the preceding two Tree
tests and the Grid test. It runs through the nonactivating native harness, retains each failure/outlier and stops
at that inventory. Plan/completion and child metadata use `.receipt.txt`; logs remain their original UTF-8 bytes.
The before/after v2 identity binds the source, actual build inputs/tools/dependencies and artifacts; the control
executable receives an additional explicit SHA-256. These filtered tests supplement required profile validation.

The fixed inventory completed on clean `cc77bec2` in 265.427 seconds: ten isolated children take 21.214-21.396
seconds each, and the prefix child takes 39.606 seconds. All eleven distinct processes exit successfully without
timeout, drain error or output truncation, and the final identity guard matches. Their ControlTests executable SHA
is `6E4589B982E254CEF993F487D734CF50CA86AE8079AE8F43609FCDBB778969E9`; the separate canonical EmbeddedTests anchor
hash is not the ControlTests hash. Exact logs and plan/completion/child metadata are retained under `reproduction/`.

The lead and independent reviewer check all 176 semantic steps, 1,683 callback records, 836 provider attempts and
352 quiet intervals. Every unchanged-republish step has client `mark=cutoff=148` and provider `counts=74..74`,
with zero observed client arrivals and zero provider attempts. No timeline overflow or failed callback is recorded.
All four deterministic synchronization regressions pass in Debug, Release and ASan (twelve completions), and the
sixteen assertion/action bodies retain their original expected event sets. Callback durations of 0/15/16 ms are
coarse `GetTickCount64` observations; zero does not prove zero cost. The original extra notification has not recurred
in this inventory and its cause remains unproven. Successful repetitions do not establish permanent absence.
Each final diagnostic tail retains one later Text callback beyond step 16's assertion cutoff. It is recorded
separately from the unchanged-republish assertion and does not explain the archived `Selected Ligne 3` failure.

`Analyze.ps1 -PacketRoot ./reproduction` replays the declared process/argument inventory, complete child flags,
log hashes, quiet/snapshot/callback cutoffs and provider-attempt records. Late or in-flight callbacks remain explicit
observations; entry-sequence order need not equal callback-completion order. Provider attempts do not establish
payload identity or successful delivery. Replay requires the corpus `SHA256SUMS`; the packet's separate root inventory
also binds scripts and review receipts. The earlier derived analysis remains byte-identical as
`reproduction/original-analysis.receipt.txt`, with its original copy binding retained.

The lead reviews the delegated reader and independently runs all fifteen fixed source-only replay cases: two valid
observations and thirteen corruptions. The legacy reader accepts twelve malformed cases; the strengthened reader
passes all fifteen acceptance/rejection and exact-reason checks. The valid concurrent case covers reordered entry
sequences, a callback between steps, an in-flight final callback, and prior-step/before-action provider attempts.
Each synthetic copy is labeled, sealed after mutation and kept separately; all twenty-four original inputs stay
unchanged. This replay launches no native executable and does not add native repetitions or explain the original
failure. Results and both reader versions are retained under `replay/`.

From the repository root, choose a fresh output directory on each invocation:

```powershell
./Measurements/UiaSelection/2026-10-10/Invoke-Replay.ps1 `
    -AnalyzerPath ./Measurements/UiaSelection/2026-10-10/Analyze.ps1 `
    -OutputDirectory .build/reports/uia-replay-fresh
```

The documentation/gallery and design-system contracts were reviewed. These test/reporting changes have no visible
control change, so the existing published captures and design-system guidance remain applicable. Physical input,
assistive-technology acceptance and native ARM64 execution remain separate gates.
