# Hosted CD35 paired study — 25 October 2026

This older paired study is published separately from the current 2fed PR102 native run and its still-pending paired benchmark. Its public-safe JSON and CSV are byte-for-byte copies of the independently reviewed projection; the private receipt replay and source payloads remain in audit custody.

- PR head: `cd35e988b9c93e1a8c2162253c2edf457965b64e`.
- Measured synthetic merge: `e39793e34ee2cb14cb8a9f114ae22f155b7ddfce`, with first-parent base `4faf16ed647df71cd7d5623c1682738592771c3e`.
- Provider archive SHA-256: `17640CE0709808E7E68F00EF7B35CB27BDA66295D17129F41BF5676E9C509F93`.
- Independent replay matched 144 receipt hashes, 936 paired effects and 78 p-values/Holm adjustments.
- There were 69/72 unstable controls; 238/1,872 metric observations crossed bands, 1,634 held, 39/78 keys held in all controls, and none drifted in all controls.
- The producer reported zero regressions, but the verdict is inconclusive because the base policy is unqualified.

The observer recorded 6,098 samples, 38 gaps over two seconds, 6.72 ms P95 and 5,544.8 ms maximum collection duration. Its 34.55% weighted whole-host busy value is a `GetSystemTimes` aggregate including the benchmark, observer and other processes. It does not identify external contention, establish a quiet host, or prove that preemption caused observed variation. The replay found no control-pair matching defect and established no cause for the common scenario/time movement.

This packet is not a current performance acceptance. Do not pool its result with the SDK1714 run or the pending 2fed PR102 paired benchmark. No thresholds, fixtures, or bands changed.

The [lead replay](lead-replay.md) separately verifies retained receipt/executable hashes, stored-effect sign-flip/Holm arithmetic and CSV projection; the independent audit derives the effects from raw rounds.
