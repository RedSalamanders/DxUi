# Diagram spacing and task presentation

Status: DONE — scoped implementation, x64 Release library/consumer checks and hidden capture review completed

Owner: DxUi shared renderer, with explicit RedTilde adoption.

## Scope

- [x] Retain the current native artifacts and hidden captures for the supplied styled-flowchart, ER, table and task examples.
- [x] Size adjacent-rank corridors from measured edge labels and cardinalities, route labeled links inside those corridors, and retain bounded fallback for larger graphs.
- [x] Give table cells and inline code visible horizontal breathing room without changing source mapping, selection or history.
- [x] Use the current theme accent for task markers and add optional completed-task strike-through presentation.
- [x] Adopt the exact library commit in RedTilde; expose and persist the presentation preference for Compose and Split.
- [x] Add independent geometry, raster, settings and source/history regressions; review deterministic Light/Dark and scaled captures.
- [x] Update domain contracts, examples and change fragments, run scoped validation, and record remaining qualification separately.

## Scoped result

Implementation `703a96c788a79c1e7b28097226a2ca0a735f11bc` retains API revision 4. A clean independent checkout
passes the complete x64 Release build with zero warnings/errors and all 19 noninteractive native suites with
zero capability skips. Six gallery captures were regenerated and reviewed. The five static validators pass;
all 30 tooling scripts pass across the recorded full attempt and focused environment recoveries. Failed full
attempts retain their original outcomes.

RedTilde independently builds the exact pin with zero warnings/errors. Its 73 distinct native cases pass,
followed by `FULL_NONINTERACTIVE_PASSED`, 32 standard captures and 30 focused Light/Dark captures at
96/144/192 DPI. The six retained before captures and final application renders are compared in the consumer
packet. Source, selection, history, persistence and Compose/Split behavior also have native regressions.

The [shared receipt](../../../Measurements/DiagramSpacing_2026-11-03/README.md) and RedTilde's
`Measurements/DiagramSpacing_2026-11-03` packet retain source/artifact identities, first failures, capture
provenance and unpaired diagnostic limits. At validation time the implementation commits were local and
unpublished; publishing or merging them does not supply additional qualification.

## Evidence boundary

The before state is implementation `2e775e8d81ff415587a4d885366566ed89df655f`, with documentation at `ebb79b2e`.
Private, hash-verified native artifacts are retained before changes. This work does not close the existing
platform, real-input, assistive-technology or matched-performance acceptance gates in
[the qualification ledger](../WIP/ConsumerUiQualification_2026-10-11.md).
