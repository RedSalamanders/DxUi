# Native text-layout cost isolation

Source `1315b4fa5748ec34b146a659edf9a94a928cca57`, x64 Release, the
`MenuTextLayoutResources` diagnostic in native CI `35659847692`. This diagnostic
passes with all 384 phase records and no heap errors. The overall Release job
**failed** the independent Accessibility disclosure subscription at
`ElementFromHandle`, `E_PENDING`, 3,000 ms; its failure log/receipt are retained.
Do not treat the passing diagnostic as qualification of that first attempt.
The subsequent Release job retry passes, and the complete six-profile workflow
is now successful; [reviewed final receipts](../native-ci-diagnostics/README.md)
retain the distinction. No product repair is claimed for that intermittent timeout.

Twelve French labels use Body 13-DIP text and twelve distinct French parent paths
use Small 11-DIP text, at 396 DIP. Strings, formats and fixed-capacity output slots
exist before sampling. There is no menu, semantic tree, glyph drawing, graphics
readback or timing measurement. Creation applies the production wrapping,
alignment and no-trimming policy. `GetMetrics` then forces native layout work.
Primary-only, secondary-only and paired cases rotate through 32 cycles.

Last sixteen cycles, live heap delta from before, bytes (every sample equals its
median in this run):

| Twelve rows | After creation | After GetMetrics | After release |
| --- | ---: | ---: | ---: |
| Primary only | 6,624 | 226,272 | 0 |
| Secondary only | 6,648 | 227,482 | 0 |
| Both fields | 13,272 | 453,754 | 0 |

Thus each pair requires about 1.08 KiB before metrics and 36.93 KiB after metrics,
on this fixture. Twelve pairs retain about 443 KiB. Most observed live layout cost
appears during native shaping/measurement rather than C++ menu-item construction.
The measured live heap returns exactly to baseline after releasing the layouts.
This isolates the responsible API path, not individual native allocation stacks.
It does not establish total process-memory release, a leak-free indefinite soak,
or identical byte costs on other Windows/font configurations.

The whole-menu CI probe also passes its geometry checks, but background heap
changes make several before/after deltas negative. Retain those external results
without using them for an exact percentage attribution. The earlier matched local
[whole-menu comparison](../local-reflow-v4/README.md) is a separate machine/run;
its approximately 38-KiB incremental described-row scale is consistent with this
isolation, not an interchangeable receipt. The local x64 Release `test.ps1` run
now reproduces every live-heap figure in the table exactly, across all sixteen
retained cycles, with zero skips and all 384 phase records. Its complete suite
receipt and raw log are retained as `local-*`; the included common performance
measurement is unpaired and does not grant resource acceptance.

No menu or grid resource tradeoff is approved. Full raw CI artifacts are under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-1315b4f-release/`. The byte-preserved
suite receipt includes executable identity and the common performance samples.
`analyze.py <path-to-test.log>` validates every phase and reproduces the summary.
