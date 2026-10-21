# Native mathematics and diagrams: evidence summary

Bounded library preview, originally implemented at runtime source
`778e8630392812c65da364c778056ff42b862f36`. The original source passes 19 noninteractive
suites in each x64 Debug, Release and ASan Debug profile (57 zero-skip passes),
all six x64/ARM64 builds, the real expected-failure ASan probe, validators and
pinned formatting. ARM64 builds do not establish native runtime qualification.

Native/embedded tests cover prepared equation/cases/matrix glyph positions,
source ranges, selection, undo, opaque oversized fallback, cached mathematical
objects and state/class diagrams. The original corpus retains 105 flowchart and
20 other-family examples; unsupported grammar keeps exact source.

[Original PR #90](https://github.com/RedSalamanders/DxUi/pull/90) and its
[CI run](https://github.com/RedSalamanders/DxUi/actions/runs/39493432735) retain
source-specific history. Original local receipts, failed diagnostic probes,
source/fixture manifests and executable hashes are preserved in the external
`pr90-math-original.zip` archive (SHA-256
`1C419C4E16FFC77631EBC295C8FCE1EC164076DC604051C80F98463707FFF242`)
and the verified pre-integration Git recovery bundle. Every archived entry was
compared against its original source bytes before Git cleanup. Raw logs and
machine reports are intentionally omitted from the merged tree.

The integration reapplies only the four math/diagram commits on the current
RichTextEditor base. Earlier source-specific passes do not qualify that combined
source. Integration CI and affected tests are separate evidence. Unpaired timings
establish no non-regression. Full language compatibility, physical IME/AT, native
ARM64 execution, WARP/device lifecycle/resource acceptance, independent performance
policy and D4c remain open in the [active plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).
Consumer evidence remains in its own repository.
