# Mermaid and shared Markdown display corrections

Status: ACTIVE

Owner steering, 3 November 2026: shared Mermaid and display defects must be fixed in canonical DxUi, rather than
only recorded in the RedTilde app review. Scope: FLOW-01 styled-label publication, FLOW-02 relationship fan ports,
duplicate unordered-task bullets and code-chrome ink. Correct RedTilde's stale three-family documentation and
samples, then adopt an exact immutable tested library commit. This is a defect pass, not open-ended Mermaid syntax expansion.

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
- [ ] Complete broader profile, native platform, physical/foreground and performance qualification with their existing owners.
- [ ] Commit the immutable library change, update RedTilde's exact pin and validate consumer behavior/captures.
- [ ] Correct the prior sample/support claims and route remaining qualification with exact identities.

Local raw baseline: `.build/mermaid-display-20261103/`. `baseline-reproduction.json` records the original CPU
results, `probe/` owns its source/project/executable and `baseline-performance.json` retains native round data.
Build log: `baseline-build.log`. No user app or desktop input was taken over.

The candidate complete x64 Release build has zero warnings/errors. All four new native regressions pass;
the source-bound evidence and the retained first build/tooling refusals are indexed in
[the measurement packet](../../../Measurements/MermaidDisplay_2026-11-03/README.md).

The current pin already supports bounded flowchart, state, class, sequence, pie, ER, Gantt and mind-map profiles.
The earlier RedTilde rendering review incorrectly treated the latter five as unavailable; its documentation,
public examples and capability descriptions need correction. Advanced constructs outside each named grammar
remain exact editable source. This work does not close quiet-host/performance-policy, native ARM64, physical
IME/AT, authorized interactive or release gates. The production review's other findings remain with their owners.
