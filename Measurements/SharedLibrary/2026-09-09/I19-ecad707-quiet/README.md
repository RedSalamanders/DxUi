# Repeated baseline variance and paired measurements

All fourteen receipts use the unchanged DxUi complex WARP fixture at d192e47
(baseline) or ecad707 (candidate). Each receipt contains exact source, executable,
fixture and environment hashes. Each profile has one reference baseline followed
by three baseline/candidate pairs. Local compiler/linker activity had completed.
The original performance entrypoint and investigation thresholds are unchanged.

All six baseline-versus-baseline comparisons exceed investigation bands. All three
Debug candidate comparisons flag dirty composition (+7.9% to +11.0%); Release
results are mixed. No result was omitted. The individual comparison JSON files
preserve their original verdicts; unpaired reference receipts remain unpaired.

This series establishes measurement variability, not acceptance or a confirmed
causal regression. The separate CPU-subset series investigates the timing signal
with alternating order. Neither series replaces the original retained baseline.

See [the qualification report](../README.md) for interpretation and remaining gates.
