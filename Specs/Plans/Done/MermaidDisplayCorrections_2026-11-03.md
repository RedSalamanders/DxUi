# Mermaid and shared Markdown display corrections

Status: DONE — scoped implementation and exact-pin consumer validation closed; broader acceptance remains separately owned

Final implementation: `2e775e8d81ff415587a4d885366566ed89df655f` (API 4). The complete x64 Release build has
zero warnings/errors; all 19 selected noninteractive native suites pass with zero capability skips. The corrected
gallery's five themes and embedded example were regenerated and reviewed. RedTilde independently builds this pin
and passes 73 native cases, 32 standard captures and 18 final Light/Dark/96/144/192-DPI sample captures, including
Source/Split/Compose/F2, note edits and undo/redo. Exact bytes, retained failures and scope limits are in the
[syntax evidence packet](../../../Measurements/MermaidSyntax_2026-11-03/README.md) and RedTilde's corresponding packet.
At this validation checkpoint the implementation commits were local and unpublished; no remote CI or release result is implied.

Owner steering, 3 November 2026: shared Mermaid and display defects must be fixed in canonical DxUi, rather than
only recorded in the RedTilde app review. Scope: FLOW-01 styled-label publication, FLOW-02 relationship fan ports,
duplicate unordered-task bullets and code-chrome ink. Correct RedTilde's stale three-family documentation and
samples, then adopt an exact immutable tested library commit.

Owner clarification on 3 November: fix defects **and expand Mermaid syntax**. The validated defect checkpoint
is `33ce97ce2b29aed4eadb9270bf66e7080d56b9fc`; RedTilde's exact-pin Release run passes 73 native cases and
32 standard hidden captures, with 20 additional Light/Dark/DPI captures. Keep that checkpoint separate from
the new syntax implementation and its eventual consumer pin.

The expanded `9ed15504` checkpoint passes all 19 noninteractive Release library suites and the consumer's
73 native cases/32 standard captures, plus 18 Light/Dark/DPI sample captures. Actual capture review exposed
a further display defect: default diagram connections and frames inherited faint decorative border ink.
An offscreen editor regression measures central connection distance 16 from its background before the fix.
The final pin corrects the retained editor's content-stroke choice and preserves explicit export drawing and
authored palettes. The verified syntax checkpoint remains independently preserved.

## Bounded syntax expansion

Extend the existing native pipeline with sequence actors and plain aliases; left/right/over notes in source
order; bare `autonumber` with notes excluded from the count; solid/dotted messages without arrowheads; and
directionless `flowchart`/`graph` headers defaulting to top-to-bottom. Verify the pinned Mermaid 11.4.1
reference and current official documentation; custom start/increment numbering belongs to a newer syntax
profile and is not implied. Keep activations, control fragments, interaction/configuration and other excluded
forms as exact fenced source with diagnostics.

- [x] Define bounded private model admission and explicit timeline-note geometry without adding a note vector or draw allocations.
- [x] Add parser, independent geometry, actor/note/number raster, source/history/accessibility and cancellation/resource regressions.
- [x] Update owning contracts, capability descriptions, public examples and gallery; preserve negative examples for unsupported forms.
- [x] Validate and commit the library, then adopt an independently built immutable pin and fresh app evidence.

Private fixed-baseline location and hashes are recorded under `.build/mermaid-syntax-expansion-20261103/`.
The baseline includes the clean library commit, its source archive, all native output bytes and the consumer's
dirty source overlay; nothing in the earlier folder-navigation work is discarded.

## Evidence and execution

- [x] Inspect the current source and preserve RedTilde's existing dirty-file identities before adoption.
- [x] Build clean x64 Release at `f2e328a0`: zero warnings and errors.
- [x] Retain an unchanged-library CPU reproduction: all three flags are true for an invalid styled-label span,
  rejected ordinary ER fan and duplicate task marker. The reproduction is not qualification.
- [x] Capture the clean independent complex-UI benchmark before production edits; it is an unpaired baseline only.
- [x] Implement shared model admission, label clipping, bounded relationship-only endpoint staging and foreground code ink.
- [x] Add regressions for source ranges, actual attachments/cardinality separation and light/dark code-chrome pixels.
- [x] Validate x64 Release changed paths: EditorControls, Embedded, Accessibility and Rendering pass without skips;
  WARP/device-loss/clean-hidden assertions stay enabled, and all six regenerated gallery images were reviewed.
- [x] Route broader profile, native platform, physical/foreground and performance qualification to the [shared active ledger](../WIP/ConsumerUiQualification_2026-10-11.md); those gates remain open.
- [x] Commit the immutable defect fix, update RedTilde's exact pin and validate consumer behavior/captures.
- [x] Correct the prior sample/support claims and route remaining qualification with exact identities.

Local raw baseline: `.build/mermaid-display-20261103/`. `baseline-reproduction.json` records the original CPU
results, `probe/` owns its source/project/executable and `baseline-performance.json` retains native round data.
Build log: `baseline-build.log`. No user app or desktop input was taken over.

The original defect checkpoint's complete x64 Release build has zero warnings/errors. Its four native regressions pass;
the source-bound evidence and the retained first build/tooling refusals are indexed in
[the measurement packet](../../../Measurements/MermaidDisplay_2026-11-03/README.md).

The earlier pin already supported bounded flowchart, state, class, sequence, pie, ER, Gantt and mind-map profiles.
The earlier RedTilde rendering review incorrectly treated the latter five as unavailable; its documentation,
public examples and capability descriptions are corrected. Advanced constructs outside each named grammar
remain exact editable source. This work does not close quiet-host/performance-policy, native ARM64, physical
IME/AT, authorized interactive or release gates. The production review's other findings remain with their owners.
