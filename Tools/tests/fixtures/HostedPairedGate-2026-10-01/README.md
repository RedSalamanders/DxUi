# Hosted A/A fixtures

The summaries and same-binary control comparisons of the two hosted A/A runs retained on 1 October 2026, which
`Tools/tests/Test-BenchmarkGate.ps1` reads to check the gate's conclusion on real hosted data. They are copied from the
archives of the [retained packet](../../../../Measurements/HostedPairedGate/2026-10-01/README.md), which leave the
repository with the other raw evidence (Q4 and Q11 of the [10 October review](../../../../Specs/Plans/WIP/CodeReview_2026-10-10.md)).

| Fixture | Archive | SHA-256 of the archive |
|---|---|---|
| `aa-1` | `Measurements/HostedPairedGate/2026-10-01/reports.zip` | `ca29696301854f15ce5781f78b050378220c9361abef2f94699ec31e1b2e4aa2` |
| `aa-2` | `Measurements/HostedPairedGate/2026-10-01/aa-2/reports.zip` | `c5cd5289e6f60e4f41fc5d68c007f3bcfc368f0c7ba191167f1b42902bfc91ce` |

Each holds the archive's `summary.receipt.txt` and the 18 `*-control.comparison.json` files its scenarios name, their
content as the archive holds it (git keeps LF line ends): the gate reads nothing else of a run. The run receipts and pair comparisons stay in the archives.
