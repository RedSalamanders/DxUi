# Original unpaired canonical diagnostics

These four reports are the original canonical complex-UI measurements produced
by root test runs. They are retained with every sample and their original
comparison receipts. All comparison statuses are `unpaired`: none is a matched
baseline/candidate acceptance result or a resolution of the existing HOLD.

| Report suffix | Configuration | Invocation |
| --- | --- | --- |
| `2513c70d8fd247cb857d3f96a8fda722` | ASan Debug | Frozen Full ordinary qualification |
| `28f50a567bfc44c5a0d96ebd1dd63d68` | Debug | Frozen Full ordinary qualification |
| `e3fd854cf43e44fe85fb545d9ac5d0f1` | Release | Frozen Full ordinary qualification |
| `a81822d80bdc496aa503168c83b97278` | Release | Explicit five-trial editor diagnostic |

The Full invocation in each profile shares its single canonical report across
the 19 ordinary native suite receipts. The explicit diagnostic also triggers
the root runner's canonical measurement; it is separate from the five editor
preparation samples. Do not pool these observations or infer a speed change from
them. All retain library source fingerprint `4927C4EE` and canonical benchmark
fingerprint `E9DE4125`, whose full values and exact sources/artifacts are bound in
[the packet inventory](../inventory.receipt.txt).

The separate WARP execution-policy study uses different instrumentation and
eight fixed command-control windows. Its raw reports/captures and interpretation
are in [the packet explanation](../README.md). Neither that intervention nor
these unpaired canonical diagnostics change the production device, benchmark
closure, judge, threshold, baseline or consumer pin.
