# Rejected eight-slot cache experiment

The production cache remains **32 slots**. Reducing retained entries did not remove
the original memory increase or establish an improvement worth changing the qualified
implementation. No threshold, original baseline, or earlier failed comparison is replaced.

A fresh 32-slot measurement preceded the one-line eight-slot experiment; its patch is
retained. Release rebuilt successfully and Grid/Embedded/Rendering passed (zero skips).
Exact raw suite, executable and linked performance receipts remain in
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-grid-cache8-correctness/`.
After rejection, the production change was reverted, Release rebuilt, and a final
32-slot control measured. The working tree contains no production code change.
The patch tool normalized the experimental/restored source to LF; the final canonical
checkout restores CRLF. Reports retain their exact distinct raw source fingerprints;
the restored control has the same Git source content as V11, not identical raw bytes.

| Release multiline retention | 32 slots before | 8 slots | 32 slots restored |
|---|---:|---:|---:|
| Dirty completed offscreen FPS | 122.065 | 95.418 | 121.133 |
| Dirty median private bytes | 38,801,408 | 39,636,992 | 39,538,688 |
| Six-pass private median | 33,918,976 | 34,605,056 | 33,613,824 |
| Six-pass private peak | 42,217,472 | 40,656,896 | 40,337,408 |
| Six-pass end private bytes | 33,918,976 | 35,958,784 | 33,222,656 |
| Six-pass duration | 54.56 s | 61.05 s | 55.55 s |

The unchanged single-line path varies substantially in the middle run (dirty FPS
526.48 -> 172.42 -> 518.67), so that timing delta cannot be attributed to cache
capacity. All comparison flags and all samples are retained. The eight-slot proposal
is rejected for lack of a demonstrated resource benefit; this experiment does not
establish that every timing difference is causal. Original-production retention
median was 30,005,248 bytes; the V11 resource gate and developer advice remain open.

Logs and the patch normalize trailing whitespace only; raw source hashes are in
`provenance.txt`. Reports/comparisons are copied unchanged. No new consumer pin,
native ARM64 pass, memory budget approval or overall acceptance is implied.
