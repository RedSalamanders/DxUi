# Grid text overflow: investigation evidence

These are retained original/candidate measurements, **not performance acceptance**.
Original production is `c52a8f5` (production tree of qualified `78b3de3`); only the
four identical benchmark harness inputs differ in the baseline worktree. Reports
record exact production, harness and executable hashes. V11 is the uncommitted
candidate identified by its report fingerprints; later commits do not rewrite them.

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

Developer advice and matched retention evidence remain pending. The baseline
ASan reports are unpaired until the corresponding candidate is rebuilt and run.
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

Foreground qualification remains open: the first V11 Debug Menu run passed its
executed assertions but skipped seven desktop checks. Its revised restoration
reported child focus, foreground and cursor all restored. An external-harness
follow-up now grants foreground permission to its suspended owned child before
resuming it and bounds its lifetime with a job/timeout; that follow-up is not yet
executed. No skipped run is promoted to a complete input pass.
