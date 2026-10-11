# Stopped first pilot attempt

The fixed twelve-slot driver at clean `80c34c9ffeed0fb5a5c7370f8ec8cd071046a73e` attempted one Default/no-heap
report and stopped before the other eleven slots. The native runner returned zero and verified its complete
report, capture and before/after v2 identity. The driver's additional initial-anchor check could not run because
its module import order removed `Assert-PerformanceMeasurementState` from the caller's scope. This is a driver
failure, not evidence of measured identity drift. The original completion still records the unsuccessful check.

All eight original files remain byte-identical, with only metadata/raw sidecar filenames changed to `.receipt.txt`;
`copy.receipt.txt` records each source name, retained name, length and SHA-256. The overall driver log and the
portable import regression's 0/3 before and 3/3 after logs remain alongside them. No result or outlier was replaced.
The corrected driver runs a separately declared inventory on its new candidate; this report is not pooled into it.
