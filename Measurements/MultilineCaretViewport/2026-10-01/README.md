# Multiline caret viewport qualification

Candidate branch: `codex/multiline-caret-viewport`, based on
`c5dccff08bb00c2668a72f337c0a7eaf3446f53f`. Qualification captured the working
source before commit; consumers must adopt the final exact commit explicitly. [Identity](identity.json.receipt.txt)
binds the production source, final fixture and six A/B benchmark executables.
External originals and executables remain under
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-caret-clip-20261001`.
These are independent synthetic DxUi workloads with no consumer dependency.
Run `./Verify-Packet.ps1` to verify exact packet membership and every retained
payload against `SHA256SUMS`. Archived JSON uses `.json.receipt.txt`, preserving
its original bytes; the retained measurement scripts describe the external run
layout and are not a substitute for the preserved executables.

## Correctness and scope

The nine-line production change clips only multiline caret paint to the same
snapped viewport as text/selection. It leaves caret geometry, document state,
selection, wheel scrolling, native text services and accessibility unchanged.

[Original production](baseline/final-fixture-failure.log), built with the final
fixture, paints 16 caret pixels inside and 20 outside after PageDown.
[Failing capture](baseline/final-caret-spill.png) retains that visible spill.
Candidate WARP tests cover visible, partially visible bottom-line, offscreen
above, offscreen below with selection, and restored carets, for editable/read-only
fields at 96/144/192 DPI. Offscreen carets remain inside the capture surface, so
their absent pixels test clipping rather than target boundaries. All 30 states
pass in each final x64 profile, with zero outside pixels and 3,030 Embedded checks.
[Captures](captures/partial-bottom-144-readonly.png) use the library harness.

All 20 functional suites passed in each x64 Debug, Release and ASan Debug profile;
the ASan deliberate-error detection probe also passed. Menu has nine explicit
nonactivating capability skips per profile; the other suites have none. Full
receipts are in `qualification/<configuration>` (original JSON bytes use the established `.json.receipt.txt` archive suffix). They include WARP/device loss,
clean/dirty/hidden resource behavior, native text and UIA coverage.

The final test-only strengthening keeps the wheel-scrolled caret inside the
render target. Final Embedded logs qualify that exact fixture in all three
profiles. A source-ablation rebuild relinked ControlTests too, so its final binary
hash differs from the full-suite receipt despite unchanged tested production and
control-suite source. Do not call those full suites an exact-final-binary run.
The final ARM64 Debug/Release/ASan builds include the strengthened fixture; these
are cross-builds, not native ARM64 execution.

The first candidate restoration copied an older source timestamp, letting an
incremental Debug build reuse the unchanged-production object. The retained
`qualification/stale-timestamp-Debug.log` shows that spill. Updating the source
timestamp forced recompilation; all final Embedded passes follow that rebuild.
This is a build-provenance correction, not a suppressed functional failure.

## Performance: recorded tradeoff accepted

Unchanged-source common baselines were recorded before production edits in all
three profiles. The first paired `test.ps1` invocations stopped at their performance
comparators. Their raw receipts and flags remain in `initial-paired`; subsequent
full functional runs carry explicitly unpaired benchmark receipts. They establish
correctness, not a performance pass.

One bounded ABBAABBA series per profile used four fresh original-production and
four candidate processes, with the identical final fixture and common benchmark
inputs. Every process retains five rounds of forty frames. [Raw series](interleaved/Debug-1-A.json.receipt.txt)
and [all comparisons](interleaved-summary.json.receipt.txt) are retained. Summary values are
medians of the four process medians. A was built from the original Git blob with
LF source endings; its exact source fingerprint is reconstructed from those bytes,
not borrowed from the initial CRLF-checkout baseline. No baseline was replaced.

| Remaining aggregate flag | Original | Candidate | Change |
| --- | ---: | ---: | ---: |
| Debug clean frame P50 | 0.36005 ms | 0.39065 ms | +8.50% |
| Debug dirty throughput | 479.425 FPS | 452.854 FPS | -5.54% |
| Debug dirty frame P95 | 2.3483 ms | 2.5694 ms | +9.42% |
| ASan dirty composition P95 | 0.0185 ms | 0.0200 ms | +8.11% |

Release has no aggregate investigation flag. All flagged original/candidate process ranges
overlap. Surface bytes (3,686,400), replacement peaks, composition allocations (0),
clean allocations (0), dirty allocations and hidden work remain unchanged.
Release dirty private memory is +495,616 bytes (+1.73%); the other profile/scenario
private-memory deltas are between -2.61% and +0.31%. No deterministic budget grew.

The common scene contains no TextField and never executes the changed clipping
branch. This limits causal attribution; it does not erase a measured flag.
A single additional [same-binary control](same-binary-summary.json.receipt.txt) for Debug and
ASan used the identical candidate executable for both labelled groups. ASan dirty
composition P95 still differed by +7.79% (0.01925 to 0.02075 ms), demonstrating
noise at the scale of its A/B flag. Debug grouped medians did not flag in that
control, so the cause of its A/B timing difference remains unresolved. No equivalence
or direct caret-paint throughput claim is made; thresholds stay unchanged.

On October 1 the developer explicitly accepted the reported Debug timing differences
and directed adoption: "Accept these Debug timing differences and continue". The
question identified clean-frame P50 +0.031 ms (+8.5%) and dirty-frame P95 +0.221 ms
(+9.4%), the unproven cause, and Release's absence of aggregate flags. This accepts
those recorded Debug differences, including their corresponding dirty-throughput
change; it does not claim a causal explanation or waive consumer, DPI, accessibility,
resource or other correctness gates. The ASan flag remains documented with its
same-binary noise control. Original reports and comparator failures remain intact.

## Documentation and handoff

The five-theme gallery was republished from Release and visually reviewed for
clipping, overlap and missing content. The existing multiline tile is not focused;
the focused partial-line proof is the dedicated WARP fixture. Three sheet hashes
changed on regeneration; no original visual-test baseline was replaced.
Skills, specs, dependencies and formatting checks pass. Native ARM64, consumer pin
adoption, real IME/assistive technology and consumer presentation remain separate.
Qualification did not change consumer sources/pins, the primary checkout or
independently launched applications. The library closeout has completed root review;
consumer adoption and its runtime validation remain explicit subsequent steps.
