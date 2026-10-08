# Docs-HEAD hosted paired benchmark, attempt 1

This archive preserves all 111 report files from PR #69 workflow run 37853717305, attempt 1, paired x64 Release job 113573240610, artifact 11584535018. The artifact ZIP and its 113 entries were verified against the extracted download; all report files were then copied byte-for-byte and compared to both extracted and zipped source bytes. manifest.txt records each report's SHA-256, byte length, archived filename and source path. summary.comparison.json retains the original summary bytes under the comparison-specific measurement name; verdict.txt retains the gate metadata bytes without presenting them as benchmark schema. .gitattributes disables text normalization and sets CR-at-EOL whitespace handling for the archived bytes.

## Measurement identity

- Baseline merge parent: bea676a1f8418141f61cab9326b9b910e89a14b4, source fingerprint 41423433DDF45EFBBBAFA7B35D9A16D2F9D057E4271779D4D8EAC4955C481051.
- Candidate merge ref: cc6690c34939b476f51d57757532e469482cfe4d, source fingerprint F826386B05B760C3E8FFFFEEB857B3F889398C1E10CD57AEBBB048D20AD5E54F.
- Runner: shared hosted x64 Release runner runnervmfi6oq; three ABBA repetitions, six runs per side.
- Scenarios: Default, MultilineGrid, and MultilineGridDistinct; 26 metrics per scenario.

## Attempt 1 result

**Overall verdict: Inconclusive.** MultilineGrid flagged dirty composeCpuP95Ms at +9.97% (p=0.0433) beyond the 5% band, but its same-binary controls drifted 44.2%, so the measurement cannot distinguish candidate behavior from runner noise. This is neither a confirmed source regression nor a pass. The one-job retry is archived in ../attempt2/README.md: it reports no flagged metrics and a passing no-regression-established verdict, but controls remain unstable. Keep this first flag as unconfirmed history; the retry does not establish absence of smaller effects.

Default and MultilineGridDistinct were within-noise-budget with no regressed metrics. Controls were unstable in every scenario (0/6 stable); sensitivity to smaller shifts is limited. No positive performance claim is made. The initial workflow later completed with all six native profiles, native-scope, windows-tooling and validation passing; this paired benchmark job was its only failed job. Its single retry is retained in ../attempt2/README.md.
