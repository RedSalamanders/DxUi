# Shared presentation for RedTilde, 2026-10-23

Scoped library implementation and native verification for the owner's image-2 reference. Shared controls live
here; RedTilde owns application layout, theme policy, report fixtures and product capture acceptance. The actual
consumer packet is `D:/RedTilde/Measurements/VisualFidelity_2026-10-23/README.md` in its separate repository.

Code revision: `903643b4a2bc2f6d75250ff5e6834513c96432c4`, published on `codex/redtilde-image2-fidelity` and
restored by RedTilde through its exact lock. This packet and the gallery are later documentation, not additional
native execution of their own prose.

Implemented opt-in Filled/Subtle/Outline/Tab buttons, measured leading icons and shortcut hints, frameless trees,
reserved disclosure lanes, neutral decorations, edited-state dots, thin idle Splitter seams, source-preserving
semantic heading/container spacing and optional decorative RichTextEditor focus chrome. Defaults and high-contrast,
input, hierarchy, source/history and UI Automation contracts remain explicit in the authoritative controls spec.

[Verification projection](verification.md) | [Performance records](performance.md) |
[Documentation continuation](documentation-continuation.md) |
[Generated gallery](../../docs/gallery/README.md) | [Completed scoped plan](../../Specs/Plans/Done/RedTildeVisualFidelity_2026-10-23.md)

Six SDK builds pass: x64 and ARM64 Debug, Release and ASan Debug. The three x64 profiles each pass all 19 normal
noninteractive suites with zero capability skips (57 suite executions). Real intentional ASan detection passes.
These include default/WARP rendering, device replacement, lifecycle/idle resources, native/embedded text and UIA.
The consumer separately passes six builds, 53 behavioral cases and 23 hidden capture scenarios per x64 profile.
Its canonical report capture is 1440x900 pixels at 96 DPI; 144/192-DPI variants preserve the DIP viewport.

## Performance remains open

The baseline was retained before implementation. Both subsequent Default-fixture comparisons are unpaired and
advice-required. The first reports dirty `privatePeakBytes` 26,439,680 to 26,992,640 (+2.09%). That memory flag
does not repeat; the repeat flags clean/dirty composition CPU and dirty preparation timing instead. Neither run
establishes a cause or trusted non-regression. Exact allocation and surface observations remain retained. No
baseline, threshold or policy is changed, and no new cost is accepted.

See the two original comparison projections and receipt hashes in the performance record. Raw receipts remain
private at `.build/visual-fidelity-baseline-performance.json`, `.build/visual-fidelity-final-performance.json` and
`.build/visual-fidelity-final-performance-repeat.json`. The existing library qualification/policy hold and the
consumer's prior R10 findings remain open. A paired study and attribution belong to those existing owners.

## Scope and retained attempts

The first matched capture exposed duplicate tree row geometry; paint, input, tooltip and accessibility now use
one canonical calculation. A follow-up compile failure omitted its host argument; the corrected code adds a
painted-edge and hidden-expander input regression. The first focus-chrome test omitted keyboard modality; its
fixture now uses owned-host logical Shift messages. Three attempted simultaneous tests refused the repository's
artifact lock; the final suites execute serially. Original failed/refused records and intermediate captures remain
under `.build/visual-fidelity-*` and `.build/evidence/test-runs/`; they are not replaced by later passes.

Foreground Menu/NativeTextInput/resource suites require separately agreed desktop time and are not run here.
Physical IME/touch/AT, native ARM64 runtime, trusted paired performance, CI and release qualification remain
separate. A cross-build or screenshot cannot establish those claims. Published verification is a reviewed
projection with original hashes, not a replacement for the private sealed payloads.
The initial mixed JSON summary publication was rejected by the repository's measurement schema. All five
original summary records were preserved byte-for-byte in .build/visual-fidelity-publication with their sizes
and SHA-256 inventory. The published Markdown projections supply no fabricated benchmark metadata.
