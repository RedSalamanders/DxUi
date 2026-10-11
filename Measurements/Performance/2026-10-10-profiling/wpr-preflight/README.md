# Owned tracing permission preflight

The uniquely named WPR instance requested 64 buffers of 64 KiB in memory mode, for a two-second availability check.
Start was refused with `0xc5585011`; no workload recording was produced. Metadata is retained exactly as
`preflight.receipt.txt` (the original file was `preflight.json`), alongside the exact profile and error log.
No independently owned session was stopped and no machine policy was changed. This supplies no scheduler or
dedicated-host evidence; see the [parent explanation](../README.md).
