# Final native qualification follow-up 73

Frozen [CI run 38017295946](https://github.com/RedSalamanders/DxUi/actions/runs/38017295946) executes native head
`955deeeeeeb78112a1a955b2634564804950c322` as merge `554f9c929dc1216f1feebe4063dfc5f4190ef530`, against
`bea676a1f8418141f61cab9326b9b910e89a14b4`. The lead independently checks both parents and equal merge/head trees
`3276d25e9634cced3280200666212ed66c2a6085`. This renewed run includes the extraction repair documented in
[follow-up 70](../ExtractionFollowup70/README.md); preceding follow-up 71 does not supply its runtime pass.

All three x64 profiles pass 21 complete suites with zero skips. Both x64 Release resource fixtures pass, including
the scaling fixture's verified restoring foreground lease. All three ARM64 profiles build and pass 19 complete
noninteractive suites, including all four new extraction test functions and twelve interaction scenarios. Their
Menu and NativeTextInput suites do not execute: the foreground lease refuses the warning's safe input point when
it resolves to WWAHost's CoreWindow. Foreground, keyboard focus and pointer remain unchanged. The native jobs and
aggregate gate remain failures; build and noninteractive success cannot replace ARM64 foreground execution.

All eight independent relocated consumer cases pass at this exact merge/API revision 4: Debug, Release, ASan and
annotation-disabled ASan on each native architecture. They include rendering, twelve native module ownership
checks and ten rejected pin/build mismatches. These fixtures do not update RedXe or RedSalamander product pins or
qualify consumer applications. Both sanitizer profiles detect the isolated heap-use-after-free probe.

The lead independently audits 126 suite report/log pairs, eight consumer receipts, both resource fixtures, all
four extraction completion markers in each profile and 350 timing fields from 8400 ordered native samples.
Windows tooling, Linux validators/tooling and formatting pass at this frozen source. Native artifact bytes are
absent from CI uploads: recorded executable identities and raw report/log hashes are checked, but CI executables
were not independently rehashed. The published gallery artifact matches all nine committed files; its original
generation manifest correctly remains bound to the earlier dirty `bea676a1` capture source. The upload is not a
fresh generation at `955deee`.

The paired Release study uses seed `17836711`, twelve independent ABBA/BAAB blocks per scenario and 4096 exact
assignments. The lead reproduces 144 receipt hashes, 1440 rounds, thirteen immutable harness hashes, 78 migrated
decisions and 7200 timing fields from 172800 ordered samples. Both judges have zero regression flags in this run.
The legacy judge reports one Default clean preparation improvement, with run medians 600 to 500 ns; an independent
tied-midrank dynamic program reproduces U=166.5 and p=0.007563580781285294 from all 48 Default receipts. The paired
judge has raw p=0.009765625 and Holm-adjusted p=0.25390625, yielding `within-noise`. Nine block effects favor the
candidate, two favor the baseline and one is zero. This improvement-versus-noise disagreement establishes no
causal improvement or accepted non-regression.

Controls remain unstable in 71/72 cases, with 40/54 timing/memory cells showing excursions; all 24 exact-budget
cells remain stable. The official result stays `policy-review-required`: the measured base lacks the qualified
versioned policy, and the candidate cannot seed its own trust anchor. Retain the original bands and workloads.
Earlier [follow-up 68](../PairedAndWorkflowFollowup68/README.md) Grid preparation flags and follow-up 70 Default
memory/timing flags remain unresolved. A read-only diagnostic of the actual local paired PE files finds equal
mapped-image/code sizes, excluding static image growth as an explanation for those approximately 1.05 MB working
set increases. Runtime page, graphics and process residency remain unmeasured; no flag is dismissed or rebaselined.

The developer's approved accessibility scheduling tradeoff remains recorded in the owning performance contract.
That approval does not accept these other performance findings. Controlled-host calibration and residency
profiling, provisioned native ARM64 input execution, physical IME/screen-reader/touch validation, product API 4
adoption and qualified policy/required-rule enablement remain open in the indexed production-review plan.

The three archives preserve original CI artifacts, source/provenance attestations and independent review tools.
Every archive entry is reopened against its original length and SHA-256 in `archive-manifest.txt`; no native
binaries are committed. Later edits add only prose and evidence. Their unchanged compiled-input attestation
preserves the precise runtime source distinction and supplies no new native execution or production approval.
