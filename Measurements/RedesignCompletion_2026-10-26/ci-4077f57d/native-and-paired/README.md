# DxUi native and paired CI evidence

This privacy-safe packet summarizes the archived CI run and paired evidence. It contains structured summaries and hashes, not raw workflow logs, large ZIP payloads, PE/PDB files, or the host-observation stream. This publication is not a green CI result: the paired gate failed because base-policy review is still required.

## Source binding

Workflow head `4077f57df8207eac6c39c43437feb45b19d23cf8`, measured merge candidate `646b257a4bbf6fdcf252d137d121e2d3ac246d07`, and baseline `5a6ea951527ed29c696a0be3ab7ea4171f2c153e` have distinct roles. The measured candidate tree `18be66885916f67cafd8aeb761207bf15fbd9120` equals the workflow head tree.

All six native profiles passed and their full, verifiable performance reports bind to the measured merge candidate. Raw provider test logs were replayed: each profile has 1,894 completed test cases, with matching `[START]` and `[DONE]` counts and no `FAIL` or `ERROR` markers. The native scope selection reported 17 changed paths, 12 non-documentation paths.

Both ASan profiles passed. Their dedicated ASan child is an intentional heap-use-after-free diagnostic probe: `detected=true` and exit code 1 are the expected positive control, not an enclosing profile failure. Eight external consumer reports have exit code zero; these specific reports do not qualify every downstream consumer.

## Paired study and CI gate

The paired study used 12 blocks and produced 78 metric rows: 26 each for `Default`, `MultilineGrid`, and `MultilineGridDistinct`. The reported regression and improvement counts are zero for all three scenarios. The paired verdict is still inconclusive because the base policy was not independently qualified (`policy-review-required`); the CI gate failed after the paired job failed for that reason.

`StrictControls=false` is the recorded study setting, not an additional gate failure. Control variability is separate: 62 of 72 control comparisons were `advice-required`, and 163 of 1,872 metric observations exceeded their declared bands. Those counts are not candidate-regression counts.

The observer recorded 6,425 whole-host samples. Forty-six gaps exceeded two seconds; the largest gap was 9.758 seconds. Weighted whole-host busy time was 35.79%. These aggregate counters include the benchmark, observer, and other host processes, so they do not establish a cause. The run is not a quiet-host qualification. No full focus, physical input, or native IME qualification is claimed.

## Integrity and limits

All eight provider ZIP digests were recomputed. The paired ZIP contains 471 entries. The original extracted-payload manifest and scenario-receipt manifest were replayed: all 444 extracted files and all 144 raw scenario receipt files matched their recorded sizes and SHA-256 values. Seven native performance reports and eight consumer reports were independently rehashed and checked against their source bindings. See the JSON summaries and `input-hash-index.json` for the original manifest hashes and receipts. The input-hash map is nested under `payloadReferences`; `archive-files.sha256.json` nests its entries under `files`. Each public JSON carries the same six-file `dxui-complex-ui-v2` preflight metadata, independently hashed from clean merge commit `3bcf7ed9d0aaa6afb14495f0e0a03232b6d3bfea` (tree `18be66885916f67cafd8aeb761207bf15fbd9120`). The six input hashes match all seven raw performance reports, whose measured source is merge candidate `646b257a4bbf6fdcf252d137d121e2d3ac246d07` with the same tree as workflow head `4077f57df8207eac6c39c43437feb45b19d23cf8`. This is shared preflight metadata only and does not describe the complete native, menu, or input test closure.

Only repository-relative archive names and aggregate hashes are included. Raw paths, machine names, process identifiers, raw logs, observer samples, PE/PDB outputs, and large archive payloads remain in the private audit archive and are not copied here.
