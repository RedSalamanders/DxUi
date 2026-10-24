# Design refresh integration — 2026-10-24

The owner requested commit, push and merge with the disclosed performance and qualification holds.
Integration does not accept the measured resource cost or independently qualify the performance policy.
Shared controls remain in this canonical repository; RedTilde adopts an exact API-4 commit.

The corrected source checkpoint is `c600993861a337012e2633356c3d35c5e129bbf3`. It retains the original
`38ec36be42bf693dc649f1fe6b1dfc6cf34d4064` ancestry and includes quiet document controls, reliable input ownership,
source-preserving spacing, link presentation, no-interest accessibility walks, prepared IconOnly layouts,
embedded scheduling/lifetime guards, pending-scroll behavior and warm factory-path reuse.

## Retained failure and correction

At `b7cd1ffe5a57def4c57d7889c3b6c544a63991d8`, the official x64 Release build passed with zero warnings/errors.
Its ordinary Full run completed all 19 suites with zero skips: 18 passed and Control failed in
`TestButtonPreparedIconTextLayoutMatchesDrawTextW`. The new actual-paint assertion exposed a production gap:
format-cache eviction cleared configured formats without scheduling visual preparation. The retained layout's
exact-format guard then correctly used the ordinary DrawTextW fallback.

The correction sets the existing force-all-visual flag when formats are evicted, before the same frame's owner
preparation. Embedded cache trimming now occurs before that preparation instead of after it. It adds no timer,
content preparation, geometry publication or composition work. The original native assertions remain intact;
an additional embedded regression checks one replacement layout/factory lookup, actual prepared painting, exact
BGRA parity and subsequent warm reuse. The failed producer and native records remain unchanged privately.

The failed run's completed-record SHA-256 is `C227AF864521088156278746B5A020E390046D0EF4163A0A23A290074F1079D1`;
the failing Control log SHA-256 is `18B3EAEF86F5A7695F1C885767FD11A2455651383786D318C92791DCAC28E230`.

## Evidence boundaries

Corrected source `c600993861a337012e2633356c3d35c5e129bbf3` passes the official x64 Release build with zero
warnings/errors and all 19 ordinary native suites with zero capability skips. The native and embedded eviction
regressions pass with the original actual-paint, exact pixel and replacement-count assertions.
The [reviewed derived qualification record](qualification.md) binds every suite receipt/log, the completed
producer record, build log and library bytes. It is not a complete public copy of the private producer set.

The preceding `d54522c14bd85578e48019268e08756aaf50a6dc` source passes 19 ordinary x64 Release suites with zero
skips. The [earlier visual-fidelity packet](../VisualFidelity_2026-10-23/README.md) records six builds and 57
ordinary suite passes at its own source revision. Neither packet qualifies later source revisions.

The existing hosted paired gate reported that the base policy was not independently qualified. This is separate
from the consumer's seven regressions in its last complete whole-feature comparison. No new paired measurement,
quiet-host result, cost waiver, trusted rebaseline, full platform matrix or physical-input/AT qualification is
claimed here. Final-pin Debug/ASan/ARM64 and remaining foreground/consumer/release gates stay open.
