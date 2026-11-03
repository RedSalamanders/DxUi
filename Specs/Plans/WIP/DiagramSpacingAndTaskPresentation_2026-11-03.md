# Diagram spacing and task presentation

Status: ACTIVE

Owner: DxUi shared renderer, with explicit RedTilde adoption.

## Scope

- [ ] Retain the current native artifacts and hidden captures for the supplied styled-flowchart, ER, table and task examples.
- [ ] Size adjacent-rank corridors from measured edge labels and cardinalities, route labeled links inside those corridors, and retain bounded fallback for larger graphs.
- [ ] Give table cells and inline code visible horizontal breathing room without changing source mapping, selection or history.
- [ ] Use the current theme accent for task markers and add optional completed-task strike-through presentation.
- [ ] Adopt the exact library commit in RedTilde; expose and persist the presentation preference for Compose and Split.
- [ ] Add independent geometry, raster, settings and source/history regressions; review deterministic Light/Dark and scaled captures.
- [ ] Update domain contracts, examples and change fragments, run scoped validation, and record remaining qualification separately.

## Evidence boundary

The before state is implementation `2e775e8d81ff415587a4d885366566ed89df655f`, with documentation at `ebb79b2e`.
Private, hash-verified native artifacts are retained before changes. This work does not close the existing
platform, real-input, assistive-technology or matched-performance acceptance gates in
[the qualification ledger](ConsumerUiQualification_2026-10-11.md).
