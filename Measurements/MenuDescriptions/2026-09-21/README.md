# Described-menu library measurements

The unchanged source is `78b3de389a189c7f86f611787e0489fb6d474218`. The retained
baseline-release reports and log came from `test.ps1 -Configuration Release -Platform x64
-Suites Embedded,Control,NewControls`. All three suites passed with zero skips. The performance
receipt records the executable/source identities and five-round complex-UI resource measurements.
This scene protects existing controls; it does not alone measure the new native menu rows.

Application adoption/captures belong to RedSalamander; these are independent DxUi fixtures.

## Current qualification

`qualification/debug`, `release`, and `asan-v4` retain all 16 nonactivating x64 suites,
each with zero skips. `final-x64` retains later focused checks including UIA focus,
submenu navigation and the memory probes. Final native interaction/ARM64 validation and
resource acceptance remain pending. The user requested attribution before deciding on
the additional memory; no regression waiver exists.

## Common-scene investigation

Three alternating Release pairs in `qualification/paired` show clean process-private
medians increasing by 823,296, 1,175,552 and 1,175,552 bytes. Both sources retain zero
clean C++ allocations, 2,160 dirty allocations per round, 3,686,400 surface bytes and
zero hidden work. This scene never opens a menu. The increase therefore cannot be
assigned to allocated description rows merely because the feature adds them.

The identical final phase-sampling harness was then compiled against the unchanged
library and candidate (`qualification/phases`; equal benchmark fingerprint
`879A1F6E5A06FEC67430D954F6361F59FD3711444488C82CE02B513C10A4F221`).

| Private bytes at phase | Unchanged | Candidate |
|---|---:|---:|
| Entry | 1,986,560 | 1,986,560 |
| WARP device | 6,574,080 | 6,565,888 |
| Scene initialized | 8,327,168 | 8,151,040 |
| After warm-up | 21,823,488 | 22,077,440 |
| After PNG capture | 26,066,944 | 26,316,800 |
| Final hidden state | 29,048,832 | 28,790,784 |

Clean-round median is 26,103,808 -> 27,074,560 bytes; the comparison still flags it.
This locates variation in rendering/runtime allocation activity rather than static
entry/scene storage, but does not identify its allocator or establish nonregression.
The final hidden sample reverses the difference; it is not a full process teardown.
Do not turn this one reversal into a waiver or silently replace the retained baseline.
The subsequent [sixty-cycle investigation](qualification/retention/README.md) finds nearly
identical warmed private medians (~32.8 MiB), but does not qualify timing or an indefinite soak.

## Actual menu cost

V1 retained natural widths for 24 cycles. V2 removes the surface-size confound and
extends to 96 cycles, with twelve French entries at 96 DPI, 456-by-300-DIP constraints and
identical 476-by-322-pixel captures (613,088 raw bytes). Both versions and their
fixture sources/receipts are retained. The unchanged implementation explicitly reports
described entries unsupported; only plain mode is a before/after equivalent workload.
The candidate's plain/described comparison characterizes added functionality separately.

For the final 32 V2 cycles, median paired per-cycle private-byte increments are:

| Phase minus preceding phase | Candidate plain | Candidate described |
|---|---:|---:|
| Rendered menu minus before opening | 200,704 | 1,409,024 |
| Capture minus rendered menu | 0 | 851,968 |
| Closed menu minus before opening | 4,096 | 20,480 |

The described menu adds approximately 1.15 MiB while open in this probe. It owns 24
retained DirectWrite layouts and twelve semantic UIA entries with full names; process
totals also include Windows/DirectWrite and allocator overhead, not just those objects.
The extra capture cost is a harness readback/buffer cost, not normal menu presentation.
Closed-state private ranges overlap between plain and described modes in the final
32 cycles; GDI/USER counts remain 4/8 after closing in both modes. This is evidence of
release/reuse, not a proved long-run memory bound or an allocation-by-allocation
explanation of the independent common-scene increase. Plain-menu candidate/baseline
process levels vary as well; raw runs remain available instead of selecting only a
favorable sample.

The phase probe modifies only the benchmark harness on the retained baseline; the
menu probe modifies only its test driver/header. Library source there stays at `78b3`.
The probes are included in the candidate for reproducible investigations. See the WIP
plan for remaining gates and exact next actions.
