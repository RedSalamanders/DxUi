# Common-UI reflow follow-up, serial ABBA

These four local x64 Release runs follow the first
[matched v4 comparison](../local-reflow-v4/README.md). They execute serially,
baseline1/candidate1/candidate2/baseline2, after the RedSalamander Commands process
ended and before its FileOps run started. No local native test/build competed.
Each uses the unchanged common fixture and includes its source, executable,
compiler, hardware, driver, power and fixture identities.

Baseline production is described-menu commit `356006a`; candidate production is
`d461095` (scrollbar layout reuse and the controller exit-lifetime fix). Candidate
HEAD is documentation-only `13d18ae` after test-only `1315b4f`. The baseline is
already the described-menu implementation, not the original plain-menu source.
Only `src/Controls/DxUi.Menu.cpp` differs in production between these versions.

Median of each five-round measurement:

| Run | Clean FPS | Clean private bytes | Dirty FPS | Dirty private bytes | Dirty composition p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Baseline 1 | 2,452.874 | 26,304,512 | 554.777 | 28,217,344 | 0.0366 |
| Candidate 1 | 2,454.108 | 28,368,896 | 532.690 | 29,130,752 | 0.0198 |
| Candidate 2 | 2,527.023 | 27,762,688 | 528.230 | 28,385,280 | 0.0198 |
| Baseline 2 | 2,337.036 | 26,722,304 | 544.302 | 28,291,072 | 0.0174 |

Both corresponding pairs report **advice-required**. Clean private memory rises
2,064,384 and 1,040,384 bytes. Pair 1 also flags clean/dirty frame-tail timing and
dirty preparation; pair 2 flags dirty composition. The original composition
delta does not repeat in the same direction in both pairs. Dirty throughput is
lower in both, though within the automated investigation band. All flags remain
in the raw comparisons; no threshold or baseline was replaced.

Clean C++ allocations remain zero and dirty allocations remain 2,160 per round;
composition allocations, surface storage and hidden work remain unchanged.
Those counters do not cover every native allocation. This fixture does not open
menus, so its process-memory difference must not be presented as the storage of
one menu entry. The text-layout isolation explains actual open-menu native layout
costs, not this common-scene difference. Resource acceptance remains open.

The `.receipt.txt` files retain original JSON bytes with byte-preserving attributes.
`pair1` compares candidate1 with baseline1; `pair2` compares candidate2 with the
following baseline2. External originals remain under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-reflow-local-20260921/common-abba/`.

**Archive pruning (2026-09-25):** 17 verbose logs and raw receipt copies (50 KB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
