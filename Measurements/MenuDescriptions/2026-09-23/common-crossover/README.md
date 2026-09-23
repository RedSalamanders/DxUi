# Ordinary ComplexUi menu crossover, 23 September 2026

This packet preserves a predeclared x64 Release WARP ordinary `performance.ps1` crossover of the menu baseline (A) and description candidate (B): A1, B1, B2, A2. Each run used one `-SkipBuild` benchmark invocation after a fresh variant rebuild. Both rebuild logs show `EmbeddedTests.cpp` compiled. The four benchmark reports have the same fixture identity and eight byte-identical benchmark/tool inputs. Variant A source HEAD was `fb857d44f093da3ad62a2cb0cf090e2b5bc13bef`; variant B source HEAD was `58bdf2f5b0c519a234255bd05410234cee324c16`. The 63-file library source inventories differ only in `include/DxUi/DxUi.h` and `src/Controls/DxUi.Menu.cpp`.

The original 70 evidence files are copied byte-for-byte under `raw/`. `original-file-map.receipt.txt` maps their original path, hash, and size to the archived name. The four ordinary benchmark reports stay `raw/A1.json`, `raw/B1.json`, `raw/B2.json`, and `raw/A2.json`; all other raw JSON files have `.receipt.txt` appended so the Measurements validator does not mistake metadata for benchmark reports. `raw/file-hashes.json.receipt.txt` is the original source manifest, and `SHA256SUMS` covers the staged packet payload. `raw/A/inputs` and `raw/B/inputs` retain all eight shared fixture/tool files per variant. `raw/A/library-source-hashes.json.receipt.txt` and its B counterpart retain each full library source inventory. Build logs, exit files, benchmark logs, invocation UTC/hash receipts, comparisons, driver, and source patches are all retained.

The four saved comparisons all return `advice-required` (comparator exit 1). This is the comparator's investigation status, not a failed benchmark execution: A1, B1, B2, and A2 each exited 0. The following percentages use the saved five-round medians and the existing 5% timing/FPS and 2% memory bands. Asterisk denotes a comparator regression flag.

| Metric | A1→B1 | A2→B2 | A1→A2 control | B1→B2 control |
|---|---:|---:|---:|---:|
| Clean FPS | −6.990%* | −1.077% | +2.376% | +8.884% |
| Clean frame P50 | +6.479%* | −1.271% | −3.189% | −10.236% |
| Clean frame P95 | +11.192%* | +7.997%* | −1.865% | −4.685% |
| Clean prepare P95 | +33.333%* | 0% | 0% | −25.000% |
| Clean compose CPU P95 | +2.439% | +5.085%* | −4.065% | −1.587% |
| Clean private peak | −0.045% | −1.097% | +2.413%* | +1.334% |
| Dirty compose CPU P95 | +4.762% | +4.478% | +6.349%* | +6.061%* |
| Dirty private | −0.884% | +2.883%* | −1.629% | +2.110%* |
| Dirty private peak | −1.259% | +2.953%* | −1.986% | +2.195%* |

Clean frame P95 is the only repeated A→B flag: +11.192% and +7.997%, with same-binary controls moving −1.865% and −4.685%. It remains a repeated concerning signal requiring causal review. The dirty private flags appear only in the second A→B pair, while B1→B2 itself rises 2.110%/2.195%; the first A→B pair is lower, so this crossover does not establish a stable candidate memory increase. FPS/P50 flags appear only in pair 1, with B1→B2 changing by more than 5% in the opposite direction. Clean prepare P95's 33.333% corresponds to 0.0003→0.0004 ms and does not recur. All other 26-metric comparisons remain in `raw/`, including zero-growth allocation and surface measurements.

This fixture is an offscreen WARP ComplexUi scene with blocking one-pixel readback. It measures no swap-chain presentation or vsync. Its scene name does not prove the shared host/menu code is unexecuted. The earlier opt-in heap and frame-stage diagnostics are separate evidence; their timing is excluded from ordinary acceptance. These four ordinary runs do not justify adoption, a resource waiver, a new baseline, or a claim that all timing flags are noise. Retain the existing threshold and seek a causal explanation or optimization for the repeated clean P95 difference before qualifying this menu change.
