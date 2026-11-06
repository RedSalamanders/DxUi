# Script and color-glyph preparation

Native implementation: `1352cc7fd244909e1c274b7d567485b9e98d7f1a`, API 5.
Tooling-only diagnostic follow-up: `010c649b5b73f3d212b7be723f1dc9c78d02141b`.

[Qualification](qualification.json) verifies all 680 original inputs, every retained source/native/run archive
member, six zero-warning builds and all 66 noninteractive x64 suite executions. The real ASan detector passes.
ARM64 entries are cross-builds. All six regenerated native gallery images match the previously reviewed images
byte for byte; generation provenance records the actual qualified source.

Preparation previously retired its staging raster before checking script/color support. Mixed script/emoji
paragraphs therefore fell back to source on capable targets. Preparation now caches capability once, retains
the existing unsupported-target fallback and preserves the previous publication on checked failure or
cancellation. Native and embedded tests check actual WARP pixels, observed WIC capability, controlled
`E_NOINTERFACE`, absent targets and allocation-free warm drawing. Both record 696 colored pixels here.

The original rendering probe rejects the paragraph with API-5 source `977a9fb0`. Its same two compiled objects
admit it when linked with this implementation. Original source, object, library, PDB, executable and log
identities remain bound separately from complete native qualification.

The allocation diagnostic now reports its actual twenty semantic and five geometry scenarios, derived from
the source array/shared bound. Historical output and paired verdicts remain unchanged.

Native filter preflight prints a nonzero child's captured combined output, arguments, exit and executable hash
before assertions. An owned-process regression independently supplies stdout, stderr and exit 37. Eight actual
filter cases pass in each x64 configuration against original qualified executables, matching PDBs and runtime
DLLs. These tooling checks retain their separate source identity; native C++ inputs are unchanged.

[Retained attempts](retained-attempts.json) preserve the disk-full compiler/linker attempts and rejected
`bab35e8b` cohort: twenty suites passed, but Embedded and EditorControls stopped on the fixture's incorrect WIC
interface assumption. The corrected fixture observes the actual capability and tests refusal independently.

Earlier hosted [run 42246752571](https://github.com/RedSalamanders/DxUi/actions/runs/42246752571), source
`2f0a7bb9`, passes x64 but fails ARM64's unfiltered Grid preflight before normal ARM64 suites. The old wrapper
omitted the child error, so its cause remains unknown. All eight original wrappers and its job log are retained;
production was withheld. Improved reporting does not explain that historical failure.

Durable contracts are in [rendering](../../Specs/Rendering/Rendering_EmbeddedD3D11.md),
[resource preparation](../../Specs/Core/Core_PerformanceAndResources.md),
[controls](../../Specs/UI/UI_ControlsAndLayout.md) and [validation](../../Specs/Testing/Testing_Validation.md).
[Rich-text usage](../../docs/rich-text.md) describes the fallback. The
[scoped Done record](../../Specs/Plans/Done/ScriptColorPreparation_2026-11-06.md) closes these implementation
and local functional checks. The [remaining plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md) retains
native ARM64/physical input/AT, broader legacy policy, consumer adoption and formal paired performance.
