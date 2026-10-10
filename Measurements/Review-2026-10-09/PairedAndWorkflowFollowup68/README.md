# Completed paired study and independent consumer workflow

PR run [38010688230](https://github.com/RedSalamanders/DxUi/actions/runs/38010688230) measures frozen head
`7f2e9595a26f8a84cd11f9b769abb16e70d9716f` as actual merge
`0ebf4820dce236f41e6916fa893fac29adbfcccb`, with seed `38264413`. The lead checks both parents and equal
head/merge tree. Native qualification is retained separately in [follow-up 66](../HostedQualificationFollowup66/README.md).

Independent auditors reproduce all 144 receipt hashes, 1440 rounds, thirteen immutable-source harness hashes,
the independently randomized twelve ABBA/BAAB blocks, and all 78 migrated decisions using 4096 exact assignments
and the declared 26-slot Holm family. A separate raw-timing auditor reproduces 7200 fields from 172800 ordered
samples; it imports no production receipt validator. All recorded QPC frequencies are 10 MHz, or 100 ns per tick.

The migrated judge has zero flags. The legacy judge retains two clean-prepare P95 regressions:

| Scenario | Median run P95 | Legacy U / exact p | Paired change / raw p / Holm p |
| --- | --- | --- | --- |
| MultilineGrid | 600 to 700 ns | 398 / 0.02083381096363732 | +18.9765% / 0.0087890625 / 0.2197265625 |
| MultilineGridDistinct | 600 to 700 ns | 402 / 0.015572853944467857 | +17.6310% / 0.025390625 / 0.583984375 |

The lead independently recomputes both legacy exact probabilities using the complete tied-rank distribution,
with 32247603683100 assignments, and independently checks the paired statistics. Both legacy flags remain unresolved.
Default labels agree; the two Grid labels differ. No migrated non-regression verdict is accepted as policy qualification.

Short controls are unstable in 71/72 cases: Default 24/24, MultilineGrid 24/24, and MultilineGridDistinct 23/24.
Forty-two of 54 timing/memory cells have excursions; all 24 exact-budget cells remain stable. Thirty clean-prepare
same-side controls rise beyond 5%, split 15 baseline and 15 candidate. An independent read-only reviewer agrees
after correcting their earlier count split; the lead verifies the raw scheduled run medians.

Additional pooled diagnostics retain 4800 clean-prepare samples per side/scenario. Both Grid pooled medians stay
400 ns. MultilineGrid pooled P95 rises 800 to 1000 ns; MultilineGridDistinct stays 800 ns. Default median/P95 stay
300/600 ns. These pooled fields do not replace the declared metrics. The observed 100 ns run-median increase is
one counter tick, but a small real overhead remains plausible; quantization does not establish its cause or explain
all control instability. Retain the flags and unchanged bands. Recommend controlled quiet-host calibration and
focused preparation profiling before accepting non-regression. A separate batched diagnostic would require its
own measurement-design/policy review and cannot retroactively replace this study.

Official acceptance remains inconclusive/policy-review-required because the measured base does not contain the
versioned acceptance policy. The candidate cannot introduce its own trust anchor. The proposed policy remains
unqualified; trusted policy seeding and a later normal candidate pass remain open. The aggregate CI gate fails.

Separate workflow source `76373d4549d65967ef17e9235c366520b107ac3b` runs both independent relocated-consumer steps
unless cancelled, keeping the annotation-disabled case ASan-only. Native/aggregate failures remain failures.
The lead checks consumer-owned restore/build isolation and nonactivating rendering/module fixtures. The affected
Tooling67 scope passes all five validators and complete tooling, with an independently checked receipt and workflow
digest; formatting passes. Clean committed `76373d4` passes complete native-filesystem Ubuntu 24.04.4 / PowerShell
7.6.6 validation/tooling. These checks do not establish the new hosted ARM64 consumer outcome; exact-source CI is required.

The initial broader affected run was stopped before compilation/native execution after its owned process tree
was verified. Its log and cancellation record are retained as cancellation evidence, never as a passing result.
Every archive entry was reopened and compared with its original length/SHA-256; `archive-manifest.txt` binds the
archives. No executable/library build products are retained. Physical IME/assistive-technology/touch, native ARM64
foreground completion, controlled calibration and consumer product adoption remain open. This packet grants no
production-wide approval.
