# Markdown interruption and scan bounds

Implementation: `0ffaa1a6ae43f074f60e419eaa5becc5bc3fe0b3`, API 4.
The [verified qualification record](qualification.json) binds six zero-warning builds, all nineteen
noninteractive suites in each x64 profile, their original executable/receipt hashes and real ASan detection.
It verifies 594 original compiled inputs against the actual checkout, with source identity
`0807cf6d5fd85b50400a11ee251197d685d41bc929b60c56a2bea20e25823a60`.

Standalone display-math fences now interrupt lazy quote/list continuation and table rows consistently.
Malformed HTML scans reuse four terminator facts from the existing leaf index; malformed table candidates
are rejected before reference flattening. Link destinations admit at most 32 nested parentheses, preserving
unsupported deeper input as literal source. The source, selection and editing contract stays lossless.

`MarkdownCapacityTests.DisplayBlockInterruption`, `InlineScanBudgets` and `TableReferenceBudgets` use independent
source/block oracles and logical-work counters. Hostile doubled inputs must remain within 2.5 times their
previous work and the explicit per-input bounds. The counter measures inspected source units, not CPU
instructions, vectorized loads or elapsed time. These checks do not establish paired performance acceptance.
The durable bounds live in [performance/resources](../../Specs/Core/Core_PerformanceAndResources.md),
[controls/layout](../../Specs/UI/UI_ControlsAndLayout.md) and [usage](../../docs/rich-text.md).

The [original-attempt inventory](retained-attempts.json) preserves the instrumented baseline's three failing
groups and successful diagnostic archive separately from its empty first packaging attempt. It also records
four storage failures at the frozen candidate: Release PDB, Debug incremental-link file, ARM64 compiler
temporaries and ARM64 PDB. None of those partial outputs qualifies a profile. ARM64 Debug and ASan subsequently
passed through the supported isolated-output build path, after original failed bytes and logs were retained.
Storage relocation records preserve the original archive manifests without rewriting their old locators.

Compiler-cache compression was byte preserving: the retained original object manifests were checked again.
An obsolete baseline SDK profile was cleaned only after its library and symbols matched their retained ZIP.
The linked source and recovery/test evidence remain retained. C: and D: are not independent physical capacity
on the qualification machine, so relocating output between them is not reported as reclaimed physical space.

All six native gallery sheets were regenerated at clean `0ffaa1a6`, visually reviewed, and published with
[generation provenance](../../docs/gallery/generation.json). Long-checkout tooling checks pass fourteen MSVC
setup cases and 41 evidence/NTFS checks on actual NTFS; the focused fixture repair preserves dedicated long-path
coverage rather than shortening the production checkout. Full validators and formatting remain part of closeout.

ARM64 entries are cross-builds. Native ARM64 Debug/ASan, physical input/AT, API 5 and performance-policy gates
remain in the [single active plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).

The previous diagram/math/workspace implementation has its own independently verified
[hosted Release packet](../DiagramMathAndWorkspaceCorrections_2026-11-06/hosted-qualification.json).
That earlier main qualification does not qualify this later parser candidate.
