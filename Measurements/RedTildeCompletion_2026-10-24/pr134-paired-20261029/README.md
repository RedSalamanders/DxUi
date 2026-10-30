# PR134 hosted qualification and paired result

Original run [40869356681](https://github.com/RedSalamanders/DxUi/actions/runs/40869356681) tested merge `b7d43757bd7c1c2464c5d349ff1b1e0993da69e9`, whose tree equals head `c1c207a8e1038789241ca4bc975e72581b2d9616`, against base `660b2c89fc0b1f1e4f33ef125b2705601f2117b9`.

All six native jobs passed: 126 Full-suite receipts, zero skips, both actual sanitizer detectors and all eight consumer modes. Native ARM64 results are runtime results. Native artifacts record executable hashes but do not contain the native executable bytes.

The paired build/collection step completed. The verdict publisher and final CI gate failed because the base policy is not independently qualified. The canonical conclusion is **inconclusive**, with `libraryUnchanged: false`. No performance acceptance follows.

The independent offline replay matched all 144 raw receipts, 78 metric rows, 936 block effects and 72 control vectors. Controls were unstable in 71 of 72 cases (Default 23/24, MultilineGrid 24/24, MultilineGridDistinct 24/24). Five raw regressed flags remained visible; zero rows passed the Holm regression criterion and there were no exact-budget rises. The JSON template preserves every metric row and every control status; all adverse results remain recorded.

The provider ZIP SHA256 is `D6288587F91DA03EEFC9CFE6B875F65BDB874F1250B5F71083D522FD56F5F057`. New replay uses PowerShell/.NET arithmetic and the immutable measured judge for p values; no new dependencies or native reruns were used. No policy, band, metric, baseline or source behavior was changed. No quiet-host or scheduler/GPU attribution, historical dirty-work claim, or pooling with the separate local precision diagnostic is made.

This is a reviewed privacy-safe projection. It omits machine names, account identifiers, PIDs, environment values, private paths and raw host telemetry. The complete private provider artifact, receipts and refusal history remain retained.

Lead verification independently rehashes all 144 raw receipts, checks their role/source/build identities and computes 1,728 run medians; it also binds both retained timed executable streams. The lead raw-replay digest is `5C548008FBC0EF1E2409CDE30C69F4066C90D7C43E0DFB372C7E261886734564`.

The `*.json.template` files are literal reviewed projections and their byte manifest, following the qualification-packet convention; they contain no placeholders and are distinct from native runner receipts.
