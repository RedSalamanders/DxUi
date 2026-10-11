# Corrected diagnostic source: final build matrix

All six final builds pass with the corrected control-test sources retained one directory above. The original
protected-token Debug compilation failure remains separate failed evidence; these logs describe the corrected
source. x64 subsequently completes [nineteen noninteractive suites per profile](../../setup-runs/README.md), with
zero skips. ARM64 is cross-compilation only, with no native execution here.

| Platform | Debug | Release | ASan Debug |
| --- | --- | --- | --- |
| x64 | Build passes | Build passes | Build passes |
| ARM64 | Build passes | Build passes | Build passes |

The six `.log` files retain the complete final matrix parent output. x64 state copies are byte-identical to the
actual post-native state from those three runs; ARM64 states are actual complete identity reads without executing
the ARM64 binaries. Each captured v2 build receipt has a valid digest against its captured state. Current x64 build
receipts were also republished by the affected entrypoint's successful incremental checks, with unchanged artifacts.

`copy.receipt.txt` binds twenty-two captured originals, lengths and SHA-256 values, including the historical private
retainer source. `initial-review.receipt.txt` preserves a packaging error: six progress strings were captured beside
the six correct descriptors. The root rejected that shape, retained the original, and independently checked six
distinct descriptor/state pairs and all receipt digests before publishing the corrected review. No build or runtime
outcome changed. `review-fix.source.receipt.txt` retains that correction as historical source text.

Captured identities establish their recorded bytes and scope. They do not execute foreground suites, establish
physical IME/AT acceptance, qualify a performance policy or update a consumer.
