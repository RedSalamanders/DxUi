# Opt-in common-scene frame-stage diagnostic

This packet preserves the diagnostic A1, A2, B1, B2, A3 sequence from 2026-09-23. A is the menu-intern baseline worktree and B is the menu-description candidate worktree. The archived serial driver declares that order; the parent task observed all five runs complete. Individual JSON reports do not record invocation clock time or executable hash, so neither order nor per-invocation binary identity is independently established by the reports alone.

The five fixture inputs, two source HEADs, instrumentation diff, two incremental-build exit logs, two saved executable/library SHA-256 receipts, five run exit files, five raw reports, and five raw logs are preserved below. The input snapshots and instrumentation are identical across A and B. The raw report JSON lacks `benchmarkInputs`, so these JSON files use the existing `.json.receipt.txt` archival convention and their names and hashes are mapped below. The binary receipts are post-build snapshots, not per-run hashes. These diagnostic timings are outside the ordinary performance acceptance body.

The A/A and B/B spread exceeds the bracketed A/B differences and candidate differences reverse sign. This diagnostic does not establish a stable common-scene timing regression. It does not pass or waive the ordinary paired performance gate, which remains open. The process-local heap observations are separate evidence and do not establish live menu allocations in this common scene.

The [reviewed numerical analysis](analysis-original.md) predates inclusion of the [actual serial driver](raw/run-menu-stages-20260923.ps1); its statement that the original evidence folder lacked an archived driver refers to that earlier folder. Its current-binary hash check was true at analysis time and is not a claim about later restored binaries. The reports still lack per-run hashes and clocks. [The analyzer](analyze-menu-frame-stages.py) can recompute the stage summaries from the original evidence layout. `source-inventory.json.receipt.txt` pins the input source names, lengths and hashes; `original-file-map.json.receipt.txt` maps each original name to its byte-identical archived path. `SHA256SUMS.txt` covers every packet file except itself. Raw files are marked binary in `.gitattributes`, including JSON receipts renamed to `.json.receipt.txt` without re-encoding.

## Required archived paths

| Original name | Archived path | SHA-256 | Bytes |
| --- | --- | --- | ---: |
| `@analysis/analyze-menu-frame-stages.py` | [analyze-menu-frame-stages.py](analyze-menu-frame-stages.py) | `d202f3d618c305833f135574162748b07be3d3d0f635652b9cbf4e980b52ed3e` | 13137 |
| `@analysis/menu-frame-stages-analysis.md` | [analysis-original.md](analysis-original.md) | `2bd8fe9fb1670c6c21a029fce7ea3441ee6cebf7c24054ac1dca0ccb74a21946` | 12520 |
| `@driver/run-menu-stages-20260923.ps1` | [raw/run-menu-stages-20260923.ps1](raw/run-menu-stages-20260923.ps1) | `98a44b880d8ca50d063efde68f6d6249e77a3a5145647aaa27c743de1c04adab` | 2268 |
| `A/binary-hashes.json` | [raw/A/binary-hashes.json.receipt.txt](raw/A/binary-hashes.json.receipt.txt) | `87aed81681d34185175e659df4c7078a10e64d1221c00e5a28177dd4f144baf1` | 453 |
| `A/build-exit.txt` | [raw/A/build-exit.txt](raw/A/build-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `A/build.log` | [raw/A/build.log](raw/A/build.log) | `a8ffbca9d2142f974c987c6b332db5c37ae95d4b516ac06ded5ae3cdbec9204a` | 2827 |
| `A/head.txt` | [raw/A/head.txt](raw/A/head.txt) | `e529111fd1321d0004470258ba0abd17532b50bc9cbc106e80c19c71838ba4c8` | 42 |
| `A/inputs.json` | [raw/A/inputs.json.receipt.txt](raw/A/inputs.json.receipt.txt) | `ddc4de0266f0f6de91026e6f544e7feed885e36a408a1c6dd360619d345ec1c7` | 733 |
| `A/inputs/Samples/ComplexUi/ComplexUiScene.h` | [raw/A/inputs/Samples/ComplexUi/ComplexUiScene.h](raw/A/inputs/Samples/ComplexUi/ComplexUiScene.h) | `a166ad54851cbe97cf1980f2507af07810b316c0464a981b65da0f3af96b66bd` | 4166 |
| `A/inputs/Samples/EmbeddedControls/GraphicsFixture.h` | [raw/A/inputs/Samples/EmbeddedControls/GraphicsFixture.h](raw/A/inputs/Samples/EmbeddedControls/GraphicsFixture.h) | `b242c5efa16862ccf416cc2c7a277a2e78354273ef43d3cfdf48c69bbef5c19d` | 4826 |
| `A/inputs/Tests/Embedded/BenchmarkMain.h` | [raw/A/inputs/Tests/Embedded/BenchmarkMain.h](raw/A/inputs/Tests/Embedded/BenchmarkMain.h) | `571b1a022f6e62d12ec6001c14030ecbf28b0e9352dabe3a4bd42d35fe5bc243` | 1902 |
| `A/inputs/Tests/Embedded/ComplexUiBenchmark.h` | [raw/A/inputs/Tests/Embedded/ComplexUiBenchmark.h](raw/A/inputs/Tests/Embedded/ComplexUiBenchmark.h) | `c8cfe904921582f1096e1f311ddaaaf0175baf93cccd61cdd35eccf526a19ccf` | 10775 |
| `A/inputs/Tests/Embedded/ComplexUiFrameStages.h` | [raw/A/inputs/Tests/Embedded/ComplexUiFrameStages.h](raw/A/inputs/Tests/Embedded/ComplexUiFrameStages.h) | `50de13fed8738046b4b2323f4c80134f3906cf2ed2c4fb6211530a3687f39d10` | 6649 |
| `A/instrumentation.patch` | [raw/A/instrumentation.patch](raw/A/instrumentation.patch) | `6379679991aab689db239a7002ca1ac80ea665ad654084f30500b0ef5623e780` | 5579 |
| `A1-exit.txt` | [raw/A1-exit.txt](raw/A1-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `A1.json` | [raw/A1.json.receipt.txt](raw/A1.json.receipt.txt) | `b853353f52b7ed1415767f54460410a29306c393577ab67a3a95ade77a3f6b40` | 3440 |
| `A1.log` | [raw/A1.log](raw/A1.log) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| `A2-exit.txt` | [raw/A2-exit.txt](raw/A2-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `A2.json` | [raw/A2.json.receipt.txt](raw/A2.json.receipt.txt) | `d942fe5eb793a1f28f1a96fae19e8f8df033464eb27cb32108bd8efcf9493753` | 3435 |
| `A2.log` | [raw/A2.log](raw/A2.log) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| `A3-exit.txt` | [raw/A3-exit.txt](raw/A3-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `A3.json` | [raw/A3.json.receipt.txt](raw/A3.json.receipt.txt) | `2c06302bd159cd961ecabf0566566a091118d8e1e110d33d8027026c91309c80` | 3445 |
| `A3.log` | [raw/A3.log](raw/A3.log) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| `B/binary-hashes.json` | [raw/B/binary-hashes.json.receipt.txt](raw/B/binary-hashes.json.receipt.txt) | `fb0703dbd53cea68f099767b6c766d9186a1b9ce48e99d3a490eee0c2b1838a8` | 445 |
| `B/build-exit.txt` | [raw/B/build-exit.txt](raw/B/build-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `B/build.log` | [raw/B/build.log](raw/B/build.log) | `37c30bee98da80f061abf8ede2e8d86be39759c490c8a1915ce2361d49371244` | 2795 |
| `B/head.txt` | [raw/B/head.txt](raw/B/head.txt) | `9d631dee48dfd90dd9d94ad56fde5ad03128117cac2d36fb0fd85e153ed2b633` | 42 |
| `B/inputs.json` | [raw/B/inputs.json.receipt.txt](raw/B/inputs.json.receipt.txt) | `ddc4de0266f0f6de91026e6f544e7feed885e36a408a1c6dd360619d345ec1c7` | 733 |
| `B/inputs/Samples/ComplexUi/ComplexUiScene.h` | [raw/B/inputs/Samples/ComplexUi/ComplexUiScene.h](raw/B/inputs/Samples/ComplexUi/ComplexUiScene.h) | `a166ad54851cbe97cf1980f2507af07810b316c0464a981b65da0f3af96b66bd` | 4166 |
| `B/inputs/Samples/EmbeddedControls/GraphicsFixture.h` | [raw/B/inputs/Samples/EmbeddedControls/GraphicsFixture.h](raw/B/inputs/Samples/EmbeddedControls/GraphicsFixture.h) | `b242c5efa16862ccf416cc2c7a277a2e78354273ef43d3cfdf48c69bbef5c19d` | 4826 |
| `B/inputs/Tests/Embedded/BenchmarkMain.h` | [raw/B/inputs/Tests/Embedded/BenchmarkMain.h](raw/B/inputs/Tests/Embedded/BenchmarkMain.h) | `571b1a022f6e62d12ec6001c14030ecbf28b0e9352dabe3a4bd42d35fe5bc243` | 1902 |
| `B/inputs/Tests/Embedded/ComplexUiBenchmark.h` | [raw/B/inputs/Tests/Embedded/ComplexUiBenchmark.h](raw/B/inputs/Tests/Embedded/ComplexUiBenchmark.h) | `c8cfe904921582f1096e1f311ddaaaf0175baf93cccd61cdd35eccf526a19ccf` | 10775 |
| `B/inputs/Tests/Embedded/ComplexUiFrameStages.h` | [raw/B/inputs/Tests/Embedded/ComplexUiFrameStages.h](raw/B/inputs/Tests/Embedded/ComplexUiFrameStages.h) | `50de13fed8738046b4b2323f4c80134f3906cf2ed2c4fb6211530a3687f39d10` | 6649 |
| `B/instrumentation.patch` | [raw/B/instrumentation.patch](raw/B/instrumentation.patch) | `6379679991aab689db239a7002ca1ac80ea665ad654084f30500b0ef5623e780` | 5579 |
| `B1-exit.txt` | [raw/B1-exit.txt](raw/B1-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `B1.json` | [raw/B1.json.receipt.txt](raw/B1.json.receipt.txt) | `c25db1105a7f88ad3111edc0af3b0204b1b19a86ccd9a6ef6dea0c50ffebd53d` | 3438 |
| `B1.log` | [raw/B1.log](raw/B1.log) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |
| `B2-exit.txt` | [raw/B2-exit.txt](raw/B2-exit.txt) | `13bf7b3039c63bf5a50491fa3cfd8eb4e699d1ba1436315aef9cbe5711530354` | 3 |
| `B2.json` | [raw/B2.json.receipt.txt](raw/B2.json.receipt.txt) | `d2aa0b04b47496e9fc7d1438eaa3b5101a987899a7383c6b8f38c6504769b4fa` | 3438 |
| `B2.log` | [raw/B2.log](raw/B2.log) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 0 |

Generated packet files: [.gitattributes](.gitattributes), [curator](curate-menu-frame-stages.py), [source inventory](source-inventory.json.receipt.txt), [original-name/hash mapping](original-file-map.json.receipt.txt), and [SHA-256 manifest](SHA256SUMS.txt).

Source roots for the original-name table: ordinary relative names are under `C:/RedSalamander.Perf/evidence/i26-ui/menu-frame-stages-20260923`; `@driver` is `C:/RedSalamander.Perf/evidence/i26-ui`; `@analysis` is the external Codex visualization artifact directory. All archive copies are byte-for-byte copies. This packet records diagnostic evidence only; restore the temporary opt-in instrumentation before running the standard controlled pair.
