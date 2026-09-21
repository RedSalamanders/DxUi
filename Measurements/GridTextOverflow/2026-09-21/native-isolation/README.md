# Native layout isolation: rejected experiments

Production remains V11 (`71d6446`); diagnostic starting tree `2ba0dfb`.
No consumer dependency changed. The fresh V11 control was measured before edits.
All experiments below were restored, including the functionally correct prototype.
Full external logs remain under
`C:/RedSalamander.Perf/evidence/i26-ui/grid-native-isolation-20260921`.

## Questions and results

The first two deliberately incomplete renderers isolate native calls. They are not
usable UI candidates, and their changed output cannot establish non-regression.
Five short dirty-round medians, identical final benchmark hash
`07C4055AF1AE0D67209A004CBC10C9872B9308CD137F3231A20E5E3A44A0E61E`:

| Variant | Completed offscreen FPS | Private bytes |
|---|---:|---:|
| Fresh V11 control | 115.686 | 40,062,976 |
| Suppress only multiline glyph submission | 123.422 | 33,644,544 |
| Omit only `SetTrimming` | 113.531 | 39,944,192 |
| Bounded inline tail, separator only | 113.202 | 38,449,152 |

Trimming alone did not account for the increase in this short sample. Suppressing
glyph submission lowered the sample, while substantial allocation remained; it
does not prove a particular native allocator, a leak, or an exact drawing cost.
Do not subtract these process medians and label the difference object storage.

The bounded-inline prototype keeps the existing paragraph bound, replacing the
omitted tail with one inline marker in the measured layout instead of constructing
a final display layout. Its first version fails the existing pixel assertion:
DirectWrite does not paint the inline marker when its range covers only paragraph
separators. The corrected version gives that range a literal marker glyph in the
bounded layout input. Original model/copy/UIA text is untouched.

The correction passes Release Grid, Embedded and Rendering, zero skips, including
French accents, color emoji, visible omission, complete copied text, and unchanged
surviving pixels when a cell crosses the viewport. It was then rejected on resource
evidence, not accepted solely because the simpler code passes functional tests.

## Retention comparison and limits

The corrected prototype runs the same 6,000-frame, 50-fps-paced retention workload
as the [retained original/V11 pair](../heap-attribution/README.md), with the same
benchmark hash. FPS rounds remain unpaced; scroll samples occur every 200 frames.
All heap walks report zero errors.

| Implementation | Dirty FPS | Scroll private median | Sampled private peak | Final scroll private |
|---|---:|---:|---:|---:|
| Original, retained paced baseline | 52.814 | 29,743,104 | 30,109,696 | 29,868,032 |
| V11, retained paced candidate | 122.352 | 36,831,232 | 42,332,160 | 30,523,392 |
| Corrected bounded inline marker | 115.230 | 37,914,624 | 45,936,640 | 34,308,096 |

The corrected prototype's median live heap blocks total 10,948,929 bytes and its
median free heap blocks total 16,447,448 bytes. Heap totals and process-private
memory are different measures. This single follow-up does not establish precise
regression percentages, but provides no reason to adopt the experiment or repeat
it unchanged. Neither arbitrary cache reductions nor removal of the second layout
has established resource acceptance. A native call-stack attribution is still
unavailable; the previous WPR access-denied result was not retried unchanged.

## Restoration and provenance

After restoring V11, a Release rebuild and Grid/Embedded/Rendering rerun pass,
zero skips. Library and test-harness inputs are identical to `2ba0dfb`; this change
adds reviewed evidence and checkpoint documentation only. Its existing Debug/ASan
and ARM64 cross-build qualification remains evidence for those unchanged inputs.
No new native ARM64, consumer, mixed-DPI, AT or resource pass is claimed.
Gallery inputs and production behavior are unchanged, so no gallery regeneration
is needed for this evidence-only commit. Both library plans remain ACTIVE.

The [raw packet](raw/README.md) retains exact experimental patches, failed and
passing test logs, benchmark receipts and restored-source qualification. The
ordinary common-scene measurements attached to test runs are explicitly unpaired;
they do not replace any acceptance baseline. Review this packet alongside the
separate menu investigation, never as an explanation of that feature's cost.
