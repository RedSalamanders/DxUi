# Corrected setup-diagnostic binaries

These runs use the corrected test-only inputs retained in [the source archive](../setup-diagnostics/README.md),
on dirty commit `5763d57e`. They record their exact captured inputs, artifacts and scope; earlier clean-head focus passes
cannot authorize the new binaries. [The final matrix](../setup-diagnostics/builds-final/README.md) records all six builds.

| Snapshot | Executed native scope | Result |
| --- | --- | --- |
| [Debug](noninteractive-Debug/README.md) | Nineteen complete noninteractive suites | Pass, zero skips |
| [Release](noninteractive-Release/README.md) | Nineteen complete noninteractive suites | Pass, zero skips |
| [ASan Debug](noninteractive-ASanDebug/README.md) | Nineteen complete noninteractive suites plus isolated detection probe | Pass, zero skips; expected probe diagnosis |
| [ASan confirmation](ASanDebug-attempt1-canceled/README.md) | No foreground suite executed | Start timed out; all three desktop states verified unchanged |

The following retries use clean commit `4bcc32fb`, with unchanged diagnostic source and executable bytes. Every
attempt retains complete actual post-focus provenance and its own parent report; no retry replaces earlier evidence.

| Snapshot | Executed native scope | Result |
| --- | --- | --- |
| [ASan attempt 2](ASanDebug-attempt2-canceled/README.md) | No foreground suite executed | Start timed out; all three desktop states verified unchanged |
| [ASan attempt 3](ASanDebug-attempt3-pass/README.md) | Four complete foreground suites | Pass, zero skips; foreground/focus unchanged and pointer restored |
| [Release attempt 1](Release-attempt1-canceled/README.md) | No foreground suite executed | Start timed out; all three desktop states verified unchanged |

The lead checks every current executable hash, complete suite log, scoped/build/tooling digest and full actual
post-native provenance. The private reviewer and archiver are retained as source text, with helper-copy bindings.
The initial Debug reviewer had an incorrect process-exit completion expectation; its failed source and receipt
remain alongside the corrected positive replay. A deliberate `std::exit(0)` fixture uses its own `[EXIT]` marker.

Each archive-copy receipt binds the immutable external snapshot to the repository. Raw copy receipts describe the
generated originals at capture time; later runs may replace those originals. All parent performance observations
are unpaired and separate from the fixed twelve-report pilot. No non-regression or policy qualification follows.

Fresh ASan foreground validation passes on the exact corrected binaries. The zero-size setup acquires native and
logical focus with an active session/TSF document on its first attempt, and its original assertions pass. Release
and Debug foreground validation remain open for this archived source. The later
[approved automatic-start candidate](../../auto-start/README.md) separately passes six builds, all 57
noninteractive and twelve full foreground executions in Debug/Release/ASan, zero native skips and verified
restoration. These new receipts qualify their own lease bytes; the records here stay unchanged. The original
activation/focus/session failure's cause remains unproved. Native ARM64, physical IME/assistive technology and
consumer acceptance remain separate.
