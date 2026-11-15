# Initial policy qualification owner record

ReviewRecordVersion: dxui-initial-policy-owner-review-v1
PolicySourceSha256: 3BC4542822B664139E42B4CFBAED6EE5A92AF30AFD130DEA1928D3BDB7D3D5FF
ReviewedStudyRun: 43724475279
RawArtifactSha256: 0FCE2543BDEA2B239589FBA1A164752EDF5311E616AA204C72F6DB573A3BDD2A
IndependentReplaySha256: BED5272EE3A1988C2015564396158B947EF6F0989FFA6030B27E8A04DDBB1DC2
IndependentActualReviewSha256: 9B11B83CA99FAC9F2A64D11CDFB832DCDB0D9943A9CD845364DED89B3FC7606C
OldHarnessIdentity: 05D21B361020046345D1D122713626CE400CE6115079A67D15F47E8F015B648F
QualifiedHarnessIdentity: 7249D6FCCF538E722FAC8E615CB1656BD516F9224828BBB7CDFBB64FB82C0E56

State: APPROVED
OwnerApproval: APPROVED
OwnerIdentity: eric-jesover
ApprovalUtc: 2026-11-15T17:54:51Z
ApprovalEvidence: owner-decision:call_5e592875637a49148f3998bedb83a7dd
NormativeClarificationDecision: APPROVED-ONE-TIME-CLARIFICATION
HarnessTransitionDecision: APPROVED-INITIAL-METADATA-TRANSITION
AdditionalCalibrationDecision: NO-ADDITIONAL-AA-BEFORE-SEED; FRESH-H6-CI-NATIVE-PAIRED-REQUIRED

While State is DRAFT or OwnerApproval is PENDING, this record confers no policy authority and must not be committed together with a qualified policy. An approved record is valid only after the independent owner has explicitly approved the exact bounded scope below and the permitted completion fields record that actual decision. Candidate authors, generated notes, successful workflow jobs and statistical labels cannot supply that approval.

## Fixed proposed scope

- Policy file: `Tools/PerformanceAcceptancePolicy.v1.json`, schema 1, existing policyId `dxui-complex-ui-paired-v2`.
- Base B: `4cdf1ce52c7785446698e346b8aec058ed6a9283`; existing maintenance branch `ej/api4-maintenance-4cdf1ce5`.
- Reviewed candidate H5: `144c6cbc7112fefbda58e48db5c8ac88976f94bb`.
- Proposed policy bytes SHA-256: `3BC4542822B664139E42B4CFBAED6EE5A92AF30AFD130DEA1928D3BDB7D3D5FF`.
- Only changed JSON values: `qualified=true`, `qualificationStatus="qualified"`, and the fixed qualificationNote pointing to this record. Existing judge, assignment, protocol, bands, family, minimum blocks and exact/absolute budgets remain byte-for-byte/value-for-value unchanged.
- Judge: `dxui-paired-block-sign-flip-holm-v2`; normalized source SHA-256 `12C7EA7AB2101855191B21713710633EE86AAEB635C387E59AEF71EC6AFBE97D`.
- Assignment protocol: `independent-ABBA-BAAB-v1`; normalized generator SHA-256 `4B6AD6769A74BE3CF76C46937FD8EE4F8E193E0E4E5405459C6F147F9A92AA78`.

## Exact reviewed diagnostic evidence

Manual [run 43724475279](https://github.com/RedSalamanders/DxUi/actions/runs/43724475279), attempt 1, actor `eric-jesover`, workflow_dispatch on the named API-4 head branch. Both source revisions and harness commit were H2 `12753a8fabcfd45c6f828abb5835332e2f85b74b`, clean source, one physical benchmark executable under both randomized labels. Original request interval was 2026-11-15T14:27:43.2587068Z–14:27:45.1928939Z; original input hash `c27bd28b3790966827b301a2e6d14a3a3f541aa5d4f0e0c9b5f28d12529bbc31`. Official log corroborates identical full H2 inputs, all three scenarios and calibration=true; 12 blocks were fixed in the workflow, not supplied as an input.

Actual completed measurements are 3 scenarios × 12 blocks × 4 invocations = **144 distinct raw receipts**, 78 declared decision slots and 72 same-binary controls. Scenarios: Default, MultilineGrid, MultilineGridDistinct. Seed 1003055417; literal independent schedule has 8 ABBA and 4 BAAB blocks. Measurement step ran 14:32:49–17:05:49 UTC (2h33m). Paired job succeeded, published performance conclusion remained **inconclusive / policy-review-required**, and the overall workflow failed for separate native/tooling reasons.

Raw artifact: GitHub Actions artifact 14171049687, paired-benchmark-x64-Release, 100557267 bytes, SHA-256 `0FCE2543BDEA2B239589FBA1A164752EDF5311E616AA204C72F6DB573A3BDD2A`, verified against GitHub service digest. A durable full copy and original logs are retained under the approved archive:

`Z:\RedSalamander.Perf\evidence\completed-runs\20261115T143028Z-74468-174bde528bd7409b93f39eef8803989e\scratch\producer\manual-H2-AA-monitor`

Owner report SHA-256 `EBE9400E5F8FEA266548B0C7BE18BDF70E1D3994F08D383225B42C510EB047BD`; 1073-file seal SHA-256 `E72FA761A4359FEEACE73BBD28EE1B3C9B29FFAF6040374A95CF79A05CD6C51A`; independent frozen-source replay result SHA-256 `BED5272EE3A1988C2015564396158B947EF6F0989FFA6030B27E8A04DDBB1DC2`. All sealed bytes were verified again after canonical archival. The local collector's interrupted/abandoned archive disposition is distinct from hosted paired-job success and overall workflow failure. The full ZIP must remain retained beyond hosted-artifact expiry; a live artifact URL alone is not durable custody.

Independent actual-packet review is retained at `Z:\RedSalamander.Perf\evidence\completed-runs\20261115T171734Z-37728-c08e2d2a9ebe4a36b803782767af02be\scratch\independent-AA-policy-review\independent-review.md`, SHA-256 `9B11B83CA99FAC9F2A64D11CDFB832DCDB0D9943A9CD845364DED89B3FC7606C`. It finds the packet reviewable with no validated retention repair, while explicitly recommending neither policy approval nor a noise conclusion; the bounded harness transition remains an owner decision.

## Findings the owner must assess

All 144 raw receipt hashes, complete 78-slot legacy/migrated/consumed decisions, literal seeded schedule and 72 scheduled controls independently reproduce from frozen H2 source. All 78 set decision labels are within-noise; all three set labels are within-noise-budget. These labels do **not** establish runner noise or approve a policy.

**72/72 control comparisons are unstable**, with **313/1872 per-slot observations** outside the unchanged absolute 5% timing / 2% memory bands. Both directions count. There is no invented numeric A/A pass or rejection threshold. Examples illustrate scale:

- MultilineGridDistinct dirty CPU composition p95: 0.0171→0.0382 ms (+123.3918%, 17.1→38.2 microseconds).
- MultilineGridDistinct clean preparation p95: 0.0006→0.0012 ms (+100%, 0.6→1.2 microseconds).
- MultilineGridDistinct dirty frame p95: 13.6573→6.2852 ms (-53.9792%).
- MultilineGrid clean private bytes: 29736960→27877376 (-6.2534%); working-set peak: 43835392→41431040 (-5.485%).

All **24 exact-budget slots** are within both median and raw-round peak budgets with zero deterministic rises. Surface/replacement bytes stay 3686400; clean C++ and composition allocations stay zero. Dirty C++ allocations are 2160 per 40 frames for Default, 1080 per 40 for each multiline scenario; all remain below the fixed Release ceiling of 64/frame. No budgets changed.

Default hosted invocation does not request StrictControls. That explains why non-regressed set labels can coexist with unstable controls; it does not justify choosing a new strictness setting, hiding observations or concluding that the host caused the swings. The current unqualified base forces the overall inconclusive label. Future PRs still need their actual fresh paired evidence under the existing invocation and contracts.

## Provenance and limits

Single executable SHA-256 `474BDDA13F03F604BBE9F2130FDD172D6FED915D76CB469516415640A0F508BE` matches every receipt. All 12 retained EXE/LIB/PDB streams independently reproduce buildArtifactIdentity `e3a8dd0075ef41b78f83749380ac7cf648f895f062eefe8edbe8a597adeaa303`. The actual v2 build attestation, generic receipt envelopes and all raw roles bind consistently; envelope version 1 is intentional and is not an old-v1-identity defect. Full source/dependency/compiler/SDK preimages were not all retained/recomputed offline; this review does not claim a rebuilt toolchain. Disposable intermediate preimages do not govern these final-only SkipBuild measurements.

Host observations have 9064 sequential samples and matching completion SHA-256, maximum gap 1.5712049 seconds, collection p95 4.3704 ms / maximum 475.2827 ms. These observations establish no dedicated-host condition, scheduler delay, GPU duration, causal host interference or runner-noise approval.

The H2 native cohort is separate: five native profiles passed, ARM64 Release Menu failed; all eight public-consumer checks passed. Later candidate/native cohorts and RedSalamander adoption require their own exact-source acceptance.

## Initial-seeding harness decision and conditional completion

The policy JSON is a **full harness input**. Old policy SHA-256 `B1BB32FE22BB744654163F563E53755BAB10CE79561964EB3E200B37770FB582` becomes proposed `3BC4542822B664139E42B4CFBAED6EE5A92AF30AFD130DEA1928D3BDB7D3D5FF`. Exact H5's 20-input harness hash reproduces actual H2 `05D21B361020046345D1D122713626CE400CE6115079A67D15F47E8F015B648F`; substituting only these proposed policy bytes gives `7249D6FCCF538E722FAC8E615CB1656BD516F9224828BBB7CDFBB64FB82C0E56`.

The two full harness hashes above describe actual H2/H5 and the proposed integrated H6 **20-input** measurement harness. Seed P is an authority commit from B and retains B's **19-input** harness inventory, which does not include ConsumerBuild; this record does not compute P's full harness digest or claim P/H6 harness equality. A completed H2 dataset is not relabelled as either P's or H6's calibration. Fresh normal paired qualification must retain and validate the actual measurement identities under the existing harness-overlay and compatibility contracts.

The immutable B contract requires manual inspection of same-source A/A for the same runner/toolchain/harness/scenarios before first seeding. It provides no explicit hash-equivalence exception for qualification metadata. Therefore this record makes no identical-harness claim and no automatic waiver. The independent owner must explicitly decide whether the narrowly bounded initial-seeding metadata transition is covered by their review, or require a fresh complete diagnostic A/A under the reviewed final harness before qualification. The companion amendment to the initial-seeding paragraph defines that one-time owner-review route and takes authority only after independent approval and normal commitment. The owner decision must cover both that bounded normative clarification and initial qualification. The code does not independently resolve that manual bootstrap interpretation. If the owner does not accept the bounded transition, do not seed a qualified policy on this evidence; arrange the separately authorized further study without silently changing the measured base.

## Authorized completion fields after an actual owner decision

Only after approval, replace the seven PENDING fields above and State as follows; keep all fixed hashes, observations, limits and proposed policy bytes unchanged:

- - State: DRAFT→APPROVED.
- OwnerApproval: PENDING→APPROVED.
- OwnerIdentity: exact independently approving owner's authenticated or explicitly supplied identity; do not infer it from candidate authorship.
- ApprovalUtc: actual recorded approval UTC in ISO 8601; do not backdate or substitute study completion time.
- ApprovalEvidence: `owner-decision:` followed by the exact retained approval receipt/chat reference, with no whitespace in that reference.
- NormativeClarificationDecision: `APPROVED-ONE-TIME-CLARIFICATION`, only after the exact companion paragraph is independently approved.
- HarnessTransitionDecision: `APPROVED-INITIAL-METADATA-TRANSITION`, only after that exact bounded interpretation is independently approved.
- AdditionalCalibrationDecision: `NO-ADDITIONAL-AA-BEFORE-SEED; FRESH-H6-CI-NATIVE-PAIRED-REQUIRED`, only if that is the explicit owner decision. If further A/A is requested, keep this record pending and prepare a newly bound review after the actual study; this initial-transition guard does not accept an arbitrary alternative record.

Any changed policy bytes, judge, generator, protocol, bands, budgets or fixed evidence require a fresh proposal/hash review. Unknown future commit P/H6 IDs belong in subsequent delivery receipts, not invented self-referential fields in this initial record.

PolicySourceSha256 is the normalized LF UTF-8 source binding used by the cross-platform checked-in-state guard; the exact proposed JSON already uses LF, so its value equals the proposed raw policy hash. The actual retained Windows harness identities above remain raw-byte measurements and are never normalized or relabelled. The guard checks a completed bound record mechanically; it authenticates no human consent and supplies no candidate trust anchor. Synthetic TEST-ONLY fixture approvals are never the owner's real decision.

The companion test-state guard uses this exact versioned record and its fixed derived source/evidence hashes; its controlled TEST-ONLY records are never actual approval. The guard's source normalization accepts LF/CRLF UTF-8 checkout text without relabelling actual raw study hashes. It preserves unqualified validation and candidate-self-bootstrap rejection. The added test script is outside H5's 20 performance-harness inputs but can change broader build-input identities; fresh P/H6 builds and attestations remain required. Normal integration must preserve H5's existing ConsumerBuild fixture coverage. No B-only ConsumerBuild fixture change is proposed.
