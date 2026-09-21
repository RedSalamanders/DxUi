# Local matched scrollbar reflow measurements

Baseline production is `356006adb0704688d592caaea21328b7b448e188`;
candidate is `d461095efeeabf514d0a1940d58349728e7a471c` (layout reuse plus
the separately verified controller exit-lifetime repair). Both use the same v4
resource fixture, x64 Release, on the same computer, serially. Source/executable
hashes and the common benchmark's machine/toolchain/fixture identity are retained.
Test-driver hashes match except for line endings in the activation-guard header;
both byte versions are retained, and an end-of-line-insensitive diff is empty.
Baseline production is unchanged.

Both resource runs pass all 320 openings, with 476-by-322-pixel HWNDs at 96 DPI
and no heap-walk errors. The visible owned parent is fixed to one native monitor.
The authorized warning/desktop-lease wrapper reports restored child focus,
foreground and physical cursor in both runs. This fixture performs no screenshot
readback. The original failed activation-blocked v2/v3 attempts remain external;
they are not part of these passing comparisons.

## Memory result and limits

Last sixteen cycles, median rendered minus before, bytes:

| Entries / descriptions | Baseline live heap | Candidate live heap | Baseline private | Candidate private |
| --- | ---: | ---: | ---: | ---: |
| 12 / 0 | 85,462 | 85,398 | 4,096 | 8,192 |
| 12 / 1 | 150,091.5 | 150,091 | 6,144 | 4,096 |
| 12 / 2 | 188,605 | 188,669 | 4,096 | 4,096 |
| 12 / 4 | 265,698 | 265,698 | 4,096 | 4,096 |
| 12 / 8 | 419,771 | 419,771 | 14,336 | 18,432 |
| 12 / 12 | 574,501.5 | 574,501.5 | 272,384 | 18,432 |
| 24 / 0 | 94,228 | 94,164 | 4,096 | 12,288 |
| 24 / 24 | 1,072,315.5 | 1,072,315.5 | 1,185,792 | 653,312 |
| 48 / 0 | 111,476 | 111,476 | 14,336 | 10,240 |
| 48 / 48 | 2,071,406.5 | 2,071,406.5 | 2,088,960 | 954,368 |

Reusing the prepared pairs avoids allocating a second set during scrollbar
remeasurement. This pair shows lower private-memory increments for fully described
menus, but essentially unchanged live heap occupancy. One pair does not establish
a stable process-memory improvement or long-run bound. Allocator reuse makes the
private deltas non-monotonic; negative closed deltas in the raw data are preserved.

The first description adds approximately 63 KiB of live heap in this fixture,
including activation of the whole-menu semantic tree. Subsequent descriptions
add approximately 38 KiB each. Twelve described entries add about 478 KiB over
the plain menu, including native/layout-related storage. This is an inclusive
heap difference, not allocation-stack attribution. `MenuFlyoutItem` is 248 bytes
and the semantic Toggle is 760 bytes before their owned allocations. The earlier
1.15 MiB whole-process observation must not be divided by twelve and called the
menu-item object's size.

The paired common benchmark reports `advice-required`: clean composition p95
0.012 to 0.013 ms and dirty 0.022 to 0.029 ms. The complete five rounds and all
metrics are retained. A quiet repeated comparison is still required; no timing
or memory tradeoff is approved and no consumer pin has changed.

## Reproduction

`analyze.py baseline/test.log` and `analyze.py candidate/test.log` reproduce the
summaries. The analyzer rejects missing phases, incomplete variants, heap errors
and unequal geometry. Raw logs/JSON bytes are preserved by `.gitattributes`;
`sha256.receipt.txt` binds the archived inputs. Full build logs, failed probes and the
external warning wrapper remain under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-reflow-local-20260921/`.

Application screenshots and product adoption qualification remain owned by the
consumer harness. No desktop screenshot was used for this measurement.
