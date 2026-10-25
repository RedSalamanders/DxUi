# Fresh SDK actual-base comparison — 25 October

The [reviewed paired study](summary.json) retains all 78 metric rows, their 936 block effects and all 144
run-receipt digests. The fixed study uses 12 independently ordered blocks per scenario, seed `811064005`, and
the actual PR base `b06f534b36ffc2c0c9671a142d1e51545480be22`. The measured candidate is compiled merge
`8c78b5adf821daa216bee53572b3f9aae7d40ef7`, with parents that base then head
`594d67fb0fcc3829541211a7c8f100df046c87e3`; its tree matches the head's tree. The candidate library fingerprint
is `F423000D1AD799EDBAA3EE17B121480C5AE567CC75E2BCE2AF0D783AC7AB26F1`.

Independent arithmetic and the lead's repeated replay find zero regression flags. This does not qualify
performance: the measured-base policy is unqualified and 69/72 same-side controls are unstable.

| Scenario | Unstable controls |
| --- | ---: |
| Default | 22/24 |
| MultilineGrid | 24/24 |
| MultilineGridDistinct | 23/24 |

Two legacy verdicts differ from the paired/Holm result: clean `prepareP95Ms` in MultilineGrid and
MultilineGridDistinct is improved under the legacy judge and within-noise under the paired analysis. Both
judges report zero regressions. `changePercent` is 100 times the geometric mean of the block candidate/baseline
ratios minus one, rather than the difference between grand medians. Positive changes improve FPS and worsen
metrics whose preferred direction is lower; the signed block effects orient the hypothesis test toward worse.

Workflow `40177964080` completes the paired collection successfully, then fails
`Publish-BenchmarkVerdict.ps1 -Gate` with an inconclusive verdict requiring policy review. The
[six hosted native jobs](https://github.com/RedSalamanders/DxUi/actions/runs/40177964080) pass separately. No current-source AA calibration,
policy self-approval, quiet-host or acceptance claim follows from this study.

The provider ZIP digest is `97afc56fc4254cc308803d6ac3af35afbe33820c1b44ce3ac0abdcde36d8fb69`.
The lead verifies every projected metric field, all 936 block effects, all 144 raw receipt hashes and source-role
bindings, and all 24 retained EXE/PDB/library files. The exact measured EmbeddedTests executable bytes match
all 72 corresponding receipts for each role. Other retained build artifacts are inventoried; a complete source
rebuild cannot be reproduced because source/object and compiler/linker bytes are absent.

All 72 baseline receipts record a dirty checkout; all 72 candidate receipts record a clean one. The baseline
reports the clean-base fingerprint and three named common-harness overlays, but no complete dirty-path inventory
or original checkout/status is retained. A separate observer replay checks 5,438 samples and places all 144
receipt completions inside their span. It finds 28 QPC and 29 UTC sample-start periods above the auditor's
existing 2.5-second consistency bound; the largest QPC period is about 10 seconds. No free QPC gap exceeds its
10-second bound. These are consistency diagnostics, separate from benchmark acceptance thresholds.

Whole-machine weighted CPU is about 36.6% over the observer window, including build/attestation and unrelated
runner work. The reported 78.9 seconds of QPC capture cost excludes serialization, writes, sleep and final
hashing. Neither complete continuous coverage, a quiet host, total observer overhead nor scheduler/GPU isolation
is qualified. The structured packet retains every cadence breach and these limits. Raw ZIPs, reports,
binaries and superseded publication drafts remain private beneath `.build`.
