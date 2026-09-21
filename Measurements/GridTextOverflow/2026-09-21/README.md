# Grid text overflow: investigation evidence

These are retained original/candidate measurements, **not performance acceptance**.
Original production is `c52a8f5` (production tree of qualified `78b3de3`); only the
four identical benchmark harness inputs differ in the baseline worktree. Reports
record exact production, harness and executable hashes. V11 was measured before local commit `71d6446`; its exact report fingerprints
remain authoritative and later commits do not rewrite them.

V6 ABBA contains every original/candidate crossover sample, including comparisons
that require investigation. V11 Debug and Release fix complete-line ellipsis and
color emoji, and retain the original single-line drawing path. Rendering logs pass;
the color witness log retains the expected pre-fix failure.

Release multiline dirty medians: 61.974 -> 122.243 completed offscreen FPS;
private bytes 33,214,464 -> 40,226,816; working set 46,895,104 -> 49,016,832.
Per-round C++ allocations remain 1,080; composition allocations remain zero and
surface/replacement bytes remain 3,686,400. The common fixture has additional
memory/timing flags. All flags remain in the comparison JSON; faster rendering
does not waive the resource gate.

Developer advice remains pending; matched retention evidence is recorded below.
Earlier ASan reports remain unpaired; the current matched pair is retained under
[`qualification/asan-paired`](qualification/asan-paired/README.md). Its common fixture
is within noise bands; multiline still flags clean-frame memory/timing.
The six-pass retention fixture was added afterward and has a different harness
fingerprint; it must compare only with its own identical original harness.
Earlier rejected/invalid attempts and full build/test logs remain under
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-grid-*` and are not replaced here.
No hardware-presented FPS, native ARM64, consumer or overall completion is claimed.

The matched Release retention fixture completes six 1,000-row passes in 109.66 s
(original) / 55.75 s (V11). Original private bytes range 29,650,944–30,420,992;
V11 ranges 31,522,816–44,621,824 (median 36,411,392 versus 30,005,248).
Handles remain 213 throughout and drop to 211 after host teardown in both.
Surface storage remains exactly 3,686,400 bytes. Candidate private usage is
sawtoothed and ends high; this does not prove a settled long-run memory bound.
No heap purge or working-set trim is performed. The raw sequence, teardown values
and matched fixture fingerprints are retained. Developer direction remains pending.

Final V11 functional qualification: all 16 non-activating suites pass in x64 Debug,
Release and ASan Debug, zero skips, including the isolated ASan detection probe.
All three ARM64 configurations cross-build successfully; these are not native
runtime results. `qualification/` retains exact executable hashes and suite/perf
receipts. Its common benchmark receipts are explicitly unpaired; the matched
investigation reports above still govern resource acceptance. Six current gallery
images were regenerated via `gallery.ps1 -PublishDocs` and directly reviewed.

Foreground qualification now passes: Menu and NativeTextInput in all three x64
profiles, six runs with zero skips and verified original child focus, foreground
and cursor restoration. [Interactive receipts](qualification/interactive/README.md)
retain external wrapper source/hash provenance and the corrected startup ordering.
Earlier skipped/failed attempts remain recorded. Native ARM64 and resource gates
are still open; no new consumer pin or overall closeout is implied.

[Rejected eight-slot cache experiment](cache8-rejected/README.md) retains a fresh
32 -> 8 -> 32 comparison. Eight slots did not remove the memory increase; the
production change was reverted. Developer advice and original resource gates remain open.
