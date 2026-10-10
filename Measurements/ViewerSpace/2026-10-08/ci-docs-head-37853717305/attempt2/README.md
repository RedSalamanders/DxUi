# Docs-HEAD hosted paired benchmark, attempt 2

This archive preserves all 111 report files from PR #69 workflow run 37853717305, attempt 2, the single canonical retry of paired x64 Release job 113573240610. Retry job 113584719216 uploaded artifact 11585211693. The artifact ZIP contains 113 entries; all extracted bytes match the ZIP. The 111 report files were copied byte-for-byte and checked against both the extracted source and ZIP. manifest.txt records each report's SHA-256, byte length, archived name and source path. summary.comparison.json preserves the original summary bytes under the comparison-specific measurement name; verdict.txt preserves gate metadata bytes. .gitattributes disables text normalization and sets CR-at-EOL whitespace handling for the archived bytes.

## Measurement identity

- Baseline merge parent: bea676a1f8418141f61cab9326b9b910e89a14b4, source fingerprint 41423433DDF45EFBBBAFA7B35D9A16D2F9D057E4271779D4D8EAC4955C481051.
- Candidate merge ref: cc6690c34939b476f51d57757532e469482cfe4d, source fingerprint F826386B05B760C3E8FFFFEEB857B3F889398C1E10CD57AEBBB048D20AD5E54F.
- Runner: shared hosted x64 Release runner runnervmfi6oq; three ABBA repetitions, six runs per side.
- Scenarios: Default, MultilineGrid, and MultilineGridDistinct; 26 metrics per scenario.

## Retry result

**Gate verdict: pass, no regression established.** There were no flagged metrics and no regressed or improved metrics. The three scenarios were within-noise-budget. This does not establish an improvement or prove that no smaller shift exists.

Controls remained unstable: 0/6 stable for Default, 0/6 for MultilineGrid, and 1/6 for MultilineGridDistinct. Only 12/26, 8/26, and 18/26 metrics, respectively, were held by controls. Thus the retry did not reproduce attempt 1's MultilineGrid dirty composeCpuP95Ms flag (+9.97%, p=0.0433), but limited control stability means sensitivity to smaller shifts remains low. Retain attempt 1's inconclusive flag as unconfirmed historical evidence; do not call it a confirmed source regression or erase it. Under the paired retry contract this attempt passes as no regression established, with the noted limits.
