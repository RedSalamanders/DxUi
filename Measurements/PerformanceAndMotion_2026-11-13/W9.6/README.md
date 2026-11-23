# W9.6 Control footprint, part 1: the allocation prefix

Plan item `W9.6` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md), first
half: `src/Foundation/ControlAllocation.cpp` (contract:
[control object allocation](../../../Specs/Core/Core_PerformanceAndResources.md#control-object-allocation)). The second half
(cold `Control` fields in a null-means-default block) is not built; the figures below are what decides it.

## Change measured

A control's default-alignment allocation was a 24-byte header `{storage, requested, reserved}` plus up to 15 bytes of
alignment slack, with `HeapSize` read on every allocation. It is now a 16-byte `{requested, accounted}` prefix with no
back-pointer and no slack, and `HeapSize` is read only for a block allocated while `Diagnostics::nativeAllocationSink` is
set (the receipts install that sink, so they still count every control by the heap's block size). The heap's block size for
a request is the request itself (`HeapSize` reports exactly the 16 bytes plus the object size the tests observe), so each
control is 23 bytes smaller: 39 before, 16 after.

## The `G13` per-control line on `FX12`

Scenario: `FX12`, variant `form-10k` (10,000 controls in one `ScrollPanel`, 10,002 counted with the host's panel and
scroll panel: 2,000 rows of Label, TextField, Checkbox, Toggle and Button), embedded host, WARP, x64 Release, MSVC 19.52.37025.
Both receipts were written by `performance-fixtures.ps1 -Fixture FX12 -Variant form-10k -Adapter Warp -FixtureHost Embedded
-Configuration Release -Platform x64`. No hardware timing was taken.

| Receipt | Tracked live bytes after the build | Per control (10,002) | `G13` per-control line (256 B) |
| --- | --- | --- | --- |
| [before](fx12-form-10k-embedded-warp-x64-release-before.json) | 9,230,309 | 922 | exceeded |
| [after](fx12-form-10k-embedded-warp-x64-release-after.json) | 9,000,263 | 899 | exceeded |

The difference, 230,046 bytes, is exactly 23 bytes for each of the 10,002 controls. The line stays over budget by 643 B
per control on this form; the gate is therefore met in its second form: the receipt records the figure for developer advice.
The `FX5` and `FX13` recorded exceptions stay (their lines are still over 256 B).

How the two receipts were produced: the *after* receipt is the repository's own `FX12` `form-10k` receipt
(`../FX12/fx12-form-10k-embedded-warp-x64-release.json`, copied here), written by the command above on the clean
commit that carries this change. The *before* receipt is **not** reproducible from any commit: it was written on the same
machine with the one file `src/Foundation/ControlAllocation.cpp` temporarily restored to its content at `55bc01a40`
(the library otherwise identical), so its recorded source is that commit with `dirty: true`. It is kept only as the
reference for the 23-byte difference, which also follows from the two layouts (39 B against 16 B).

## Where the 899 B per control go (`FX12` form-10k, after)

Tracked live bytes after the build: 9,000,263 over 10,002 controls. Receipts count the counting `operator new` and the
checked control heap, nothing else; DirectWrite, Direct2D and GPU storage are outside it. The form is 2,000 rows of five
controls, so a row is about 4,450 B once the two container objects and the root's child vector are taken out (about 100 KB
together, 10 B per control, derived from the capacity growth of a 10,000-entry `unique_ptr` vector).
The receipt records totals only, so the split below combines the pinned object sizes (`sizeof`, Release x64, probes in
`Embedded.Tests.MemoryMetrics.h`) with what each control allocates; entries marked derived are the remainder of a total.

| Per row (five controls) | Object block (`sizeof` + 16 B prefix) | Other per-control heap blocks | Row total |
| --- | --- | --- | --- |
| `Label` ("Field 0001") | 456 + 16 = 472 | lifetime token control block 24 (`make_shared<int>` in `Control::_lifetimeToken`); text 32 (a 10-unit `std::wstring` leaves the 7-unit small buffer) | 528 |
| `Checkbox` ("Enabled") | 920 + 16 = 936 | token 24 (text fits the small buffer) | 960 |
| `Toggle` ("Sync") | 912 + 16 = 928 | token 24 | 952 |
| `Button` ("Apply") | 824 + 16 = 840 | token 24 | 864 |
| `TextField` | sizeof not probed; with its own blocks about 1,120 (derived) | token 24, text about 48 (a 19-unit string) are inside that figure | about 1,120 |
| Row | | | about 4,450 |

Reading it for the restated budget:

- Of the 899 B average, the object itself is `Control` (304 B) plus each class's own fields; the 16 B prefix is 1.8 %
  (it was 39 B, 4.2 %, before this change). The token block is a fixed 24 B and a heap-allocated caption adds 32 B or more;
  both are per control and independent of the class.
- A `Label`, the plainest class, is 528 B all-in (456 + 16 + 24 + 32); `Checkbox`, `Toggle` and `Button` are 864 to 960 B;
  the `TextField` is about 1.1 KB. The 256 B line would hold for none of them, and for a `Label` it is 272 B away.
- The 304 B of `Control` alone exceeds the line, so the line cannot be met by shrinking the allocation prefix, the token or
  strings; it needs the base object itself to fall, which is what the (not built) second half would have attacked.

## Advice for the second half

Not built (the developer restated the per-control budget instead). Estimate that informed the decision: the cold fields of
`Control` (five `std::wstring` members, two `shared_ptr<const std::function>` callbacks, the accessibility role and the
form-property pointer) are about 216 B of the 304 B `Control`, so a null-means-default block would have taken a `Label`
from 456 B to about 240 B and this form from 899 B to about 683 B per control, which is not 256 B, and a control that sets a
cold field would have cost about 220 B more.
