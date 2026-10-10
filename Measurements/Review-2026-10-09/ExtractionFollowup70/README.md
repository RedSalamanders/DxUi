# Extraction follow-up 70

The lead reproduces a surrounding-code ownership defect on the pre-change library at `65476a8`: a supported focus
or cancellation callback extracts a live Panel/TabControl into another host and focuses its requested branch, then
the older extraction continues detaching that branch. Same-host restoration can likewise leave focus or capture
pointing at a detached child. The corrected fixture fails the intended transfer assertion before implementation;
a separate focused run fails the restoration assertion. Initial protected-getter/private-cleanup fixture compile
errors are retained and supply no runtime pass. The original whole Control baseline passes before implementation.

Both extraction paths now revalidate owner/page lifetimes and host identity after callbacks, preserve newer branch
focus requests through the existing focus revision, and stop when callbacks restore branch focus or capture. The
tab path preserves metadata before aborting. No field, ABI shape, utility, composition work or periodic task is
introduced. The public ownership guidance and owning architecture contract describe the empty/no-op outcome.
Whole-host destruction within a callback remains separately unqualified. This behavior-only repair has no gallery
visual change; affected usage documentation is updated.

Eleven whole local scopes pass with zero skips: Control in x64 Debug, and Control/EditorControls/Embedded/
WindowHost/Accessibility in x64 Release and ASan Debug. All four new test functions complete in all three profiles,
covering twelve host-transfer, focus/capture restoration and capture-callback focus-choice scenarios. The lead
independently checks compiled source/dependency, build, scope, environment and actual executable identities, report/
log/performance bindings and the detected sanitizer probe. Full validators/tooling and formatting pass at their
recorded source. These focused results do not replace renewed six-profile hosted or physical-input qualification.

The original Default/Debug baseline remains unchanged. Its initial candidate comparison flags clean FPS -35.39%;
two repeats of the identical candidate executable flag -13.70% and -15.95%, with additional timing/memory findings.
All raw samples, comparison bytes and source/artifact identities remain. The lead reproduces 300 timing fields from
7200 ordered samples across the retained baseline, candidate, repeats and Release/ASan test benchmarks. The Default
fixture does not invoke either modified extraction path, so direct call cost is not demonstrated. Binary layout,
process/graphics residency or host variation are not excluded; the brief process observation does not prove quiet
execution.

The fresh interleaved Debug diagnostic uses the unchanged harness, original baseline library and tested candidate,
seed `20261010`, twelve independent ABBA/BAAB blocks and 48 raw reports. The lead verifies all 480 rounds, 26 migrated
decisions, thirteen harness hashes, actual candidate executable, explicit baseline/candidate fingerprints and 2400
timing fields from 57600 ordered samples. Both trees report dirty harness/candidate bytes; baseline library content
still matches the original clean retained fingerprint, and its before-source snapshot matches immutable Git source.

The migrated judge has zero flags; the legacy judge retains two dirty-phase memory flags. Working-set run medians
rise 45,641,728 to 46,696,448 bytes (+2.3109%, +1,054,720 bytes), and peak medians rise 45,654,016 to 46,710,784 bytes
(+2.3147%, +1,056,768 bytes). Independent tied-rank dynamic programming reproduces their exact probabilities
`0.000025285400552951` and `0.000033173354228508` over 32,247,603,683,100 assignments. Paired block effects are
+1.6159%/+1.6086%, raw probabilities `0.07666015625`/`0.07861328125`, and Holm-adjusted probabilities 1. These are
different judgments of the same raw bytes, not permission to erase either finding.

All 24 same-binary controls are unstable; all 18 timing/memory cells show excursions, while eight exact-budget cells
stay stable. The official policy remains `policy-review-required`. A process exit code of zero for this local
diagnostic supplies no accepted performance verdict. Preserve original flags, workloads and bands; hold performance
acceptance and recommend controlled quiet-host calibration and memory-residency profiling. Optimize a demonstrated
cost, reduce optional scope, or defer/revert if degradation is confirmed. No tradeoff beyond the separately approved
accessibility coalescing study is approved here.

The archives retain original baseline/red/green evidence, failed fixture compile logs, current source snapshots,
independent audits and complete paired reports. Every entry is reopened against original length/SHA-256 in
`archive-manifest.txt`; binaries are not committed. The later native source needs fresh CI. Prior frozen CI
[follow-up 71](../IndependentConsumerQualificationFollowup71/README.md) demonstrates independent ARM64 consumer
execution but cannot qualify this changed implementation.
