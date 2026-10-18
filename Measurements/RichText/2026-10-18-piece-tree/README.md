# Persistent Unicode source storage and allocation-safe admission

The bounded rich-text model now shares immutable B+ tree paths across edits,
snapshots and history. Nodes have at most 32 entries and at least 16 outside the
root. Cumulative piece/child ends index reads. A splice copies boundary paths,
reuses untouched subtrees, skips removed middle subtrees and normalizes thin
boundary chains before publication. Whole-source replacement builds one leaf;
the final root collapses redundant height. No mutable lookup cache, worker,
device, window or timer belongs to this source service.

Qualified library fingerprint:
`8EE9BD21B0CE810E1B6B39EBE30D00EF62945764CDF52D8C9AB8D65D8B8EFD99`.
Collection used the feature checkout at HEAD 02c7f2b9 with recorded dirty inputs,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges` and MSBuild node reuse disabled. The 65,536-unit
source and 128-entry history bounds remain fixed. Release Document/SourceSnapshot
sizes remain 88/16 bytes; private node layout is hidden behind shared ownership.

Source-taking Document operations now borrow `std::wstring_view` synchronously.
They validate future selections against the virtual prefix/insertion/suffix,
including new CRLF joins, before allocating source/history. A valid same-content
edit only acknowledges its exact selection. Changed payload is copied into owned
immutable buffers before return. Optional history is constructed directly by
copying existing history when needed. This avoids owning-string move proxies and
unused default-container proxies in Debug STL noexcept constructors. API revision
4 and object sizes are retained; method signatures changed, so source-coordinated
consumers must rebuild with matching headers and archive. Older built archives
are not binary compatible with these new signatures. Consumer pins are unchanged.

Before implementation, a predeclared three-slot schedule ran fixed root Release
edit/read/backward-step trials, then two isolated resource cases against that
same attested build. All three complete in 254.052 seconds within 1,200 seconds
overall / 600 per owned child. The original vector implementation retains
40,235,301 tracked C++ bytes after 4,096 interleaved edits and capped history.
Its same-content replacement requests 24 allocations / 518,630 bytes. Both
resource cases fail their predeclared budgets. Every original process receipt,
log, artifact hash, root canonical report and build binding is preserved.

The first tree implementation passes three affected Release suites. An initial
six-profile matrix then passes all 19 Release suites, but Debug exposes two
string-move iterator-proxy allocations / 32 bytes in the exact same no-op method.
The Debug root runner finishes its other suites: 18 pass and Embedded fails.
The driver stops after two profiles in 906.632 seconds; four later profiles are
unattempted. All 38 original native outcomes, the failure and schedule/completion
records remain in Raw/Intermediate and Raw. None is relabeled as final success.

After the view/history repair and borrowed-input lifetime coverage, a separate
fixed six-profile recovery completes all six in 2,039.092 seconds within 3,600
seconds overall / 900 per owned child. All **57 final ordinary x64 suite runs**
pass across Release, Debug and ASan Debug with zero native skips. Six successful
build attestations cover x64/ARM64 in all three configurations. ARM64 is
cross-compilation only. The specific isolated ASan probe actually diagnoses
heap-use-after-free; it is an expected nonzero probe, separate from suite success.

| Deterministic C++ observation | Release | Debug / ASan Debug |
| --- | ---: | ---: |
| Fragmented source + capped history retained bytes | 1,212,046 | 1,310,422 |
| Fixed retained-byte limit | 4,194,304 | 4,194,304 |
| Tracked byte delta after destruction | 0 | 0 |
| Same-content allocation calls / bytes | 0 / 0 | 0 / 0 |
| Embedded checks | 5,222 | 5,234 |
| Warm composites / C++ allocations | 1,000 / 0 | 1,000 / 0 |

The memory fixture uses an independently built 12,288-unit alternating source
from an 8,192-unit seed and 4,096 insertions. Caller seed/oracle preparation is
outside the measured lifetime. The override measures C++ storage, not Windows/
CRT-internal memory. All five operation sweeps inject failure at every observed
C++ allocation and compare text, selection, revision, saved snapshots and complete
undo/redo histories afterward. Replace/transient/reset/undo/redo have respectively
12/3/3/2/2 observed points in Release and 14/4/4/3/3 in Debug and ASan. Counts vary
because Debug containers retain iterator proxies; no fault point is silently skipped.

Each profile includes 57 named rich scenarios plus the shared Unicode corpus.
An independent flat Unicode oracle exercises split/merge, scalar/CRLF edges,
cross-node cuts and eight retained snapshots over resets. Maximum fragmentation
builds 65,536 source pieces, cuts to two surviving characters and runs 128 growth/
collapse cycles. Empty/moved snapshots, full deletion, capacity rejection,
borrowed substrings/temporaries, concurrent old-snapshot reads and complete
history preservation pass. The single-backward shortcut retains the full
766-vector movement oracle and a zero-C++-allocation mixed-script query budget.
Native and embedded rich-editor input/UIA behavior remains covered.

Five fixed within-process Release trials retain these descriptive medians:

| Operation | Before vector/value admission | After tree/view admission |
| --- | ---: | ---: |
| 4,096 fragmented edits | 202.858 ms | 16.7215 ms |
| 8,192 fragmented boundary queries | 0.7445 ms | 0.5166 ms |
| 32 single-backward queries over 61,440 units | 32.5871 ms | 17.6228 ms |

All five measured oracle/budget method bodies are byte-identical before/after.
There is one fresh native process per source, with five trials inside it. The
dormant fault-injection hook is added after the before run. No same-binary
controls, quiet-host proof, timing acceptance or sole-cause attribution is claimed.
Every preliminary/intermediate observation remains available rather than choosing
a passing repeat. These operation diagnostics do not resolve the Default frame
performance hold or measure large-payload replacement distributions.

All five validators, full tooling (including 125 profiling and four Unicode
generator cases) and clang-format 22.1.3 pass. The optional CMake readback probe
reports CMake unavailable on PATH, distinct from zero native capability skips.
Gallery inputs, glyph shaping, styles and populated examples are unchanged;
published gallery pixels are retained. No consumer repository or pin is changed.

The [clear/readback investigation](../2026-10-18-uia-boundaries/README.md) still
locates a blocking-Map boundary without a scheduler/GPU causal trace. A new
read-only token check lists no system-profiling privilege. No machine policy or
privileges are changed and no scheduler trace is invented. Frame acceptance,
physical IME/AT and native ARM64 execution remain open. The active plan continues
with incremental/shared preparation and the complete Markdown/extensions engine.

## Retention and replay

Native contains 57 exact final suite/log pairs plus the ASan probe. Build retains
six final build/intermediate attestations and original compiler logs. Performance
contains seven original canonical reports/comparisons: one before, the preliminary
Release run, initial Release/Debug, and final Release/Debug/ASan. All are unpaired.
Raw preserves every schedule/outcome, isolated failure, initial matrix, original
source overlay, first successes, inactive drafts, measured-method hashes,
descriptive trial derivation, privilege check, full validation and formatting.
Source freezes the final changed inputs. JSON receipt aliases preserve original
bytes and distinguish metadata from canonical benchmark reports.

Run `pwsh -NoProfile -File Verify.ps1 -Packet <this-directory>` to check the seal,
57 final native/build/performance bindings, six builds, original/intermediate
failures and unattempted slots, unchanged measured bodies, storage bounds, all
five fault sweeps, behavior oracles and the specific sanitizer diagnosis.
`Verify-Arithmetic.ps1` additionally recomputes all 30 descriptive observations
and six medians from the original logs. A private packet copy with one appended
native-log byte is rejected by the expected SHA-256 predicate; the original
packet remains intact. All five validators pass again after archival.
Current build products are not required for this replay. Frozen source/drafts are
replay evidence, not a second reusable implementation.
