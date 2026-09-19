# Localized action flow — baseline

Baseline at `f7fe1e4`, unchanged library with a new consumer-independent test fixture. The existing
horizontal StackPanel receives the labels' measured widths and a 32-DIP row; it cannot wrap and the
French fit assertion fails as expected. `control.log` retains five 24,000-layout batches after one
warm-up: 847, 845, 841, 850, 850 us (median 847). This diagnostic measures layout, not rendering.
The adapter is the existing StackPanel API; changing the candidate adapter does not change the
fixture's labels, DirectWrite font, widths, repetitions or fit assertions.

`fixture.h.txt` is the exact baseline input, SHA-256
`051963272CF79EE773FC71950B7BF9A523972731A5B9E4495EEF5875E08B2EB5`.
The candidate subsequently applies clang-format only to this fixture; that formatting-only
fingerprint difference must not be described as byte-identical source. Scale values model pixel
rounding at 96/144/192/96 DPI and do not establish physical monitor transitions.

`performance.json` is the simultaneous independent common complex-UI WARP baseline, with exact
compiled-input/executable identity and five raw rounds. `control-receipt.txt` preserves the native
suite receipt separately from the performance schema. Command: `test.ps1 -Platform x64
-Configuration Release -Suites Control`. Expected suite failure is not an accepted implementation.
The common fixture and this targeted workload do not complete the wider localized scene or native
mixed-DPI/application qualification.
