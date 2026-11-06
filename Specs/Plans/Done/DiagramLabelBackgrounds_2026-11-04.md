# Diagram label backgrounds and brush retirement

Status: DONE — scoped implementation, native regressions, exact-pin captures and authoritative documentation

- [x] Match ordinary arrow-label and class/ER cardinality backgrounds to the actual diagram canvas when contrast permits.
- [x] Prepare the smallest sufficient neutral lightening/darkening and preserve caller-owned ink, note/group and selection/export surfaces.
- [x] Keep contrast work, containment scans, text layout and resource creation out of paint.
- [x] Retire the retained brush with its layout; reuse its pure color and reacquire the brush from the current target.
- [x] Pass the 144-render family/theme/DPI matrix and the native cache-eviction/source-revision retirement regression.
- [x] Retain the unchanged-library retirement failure, original raster-fixture failures and superseded ASan pointer assertion with its separate unchanged-binary replay.
- [x] Pass all six zero-warning/zero-error builds, all eight affected suites in each x64 configuration, the real ASan detection probe and all 19 noninteractive Release suites across two matching runs.
- [x] Regenerate and review all six native gallery sheets, update usage/design guidance and verify RedTilde's explicit API-4 adoption through original hidden captures.

Implementation: `2a7e7c28f95a70193e4a2f68a89001d54b4d2131`, API 4 unchanged. The
[packet](../../../Measurements/DiagramLabelBackgrounds_2026-11-04/README.md) and
[qualification projection](../../../Measurements/DiagramLabelBackgrounds_2026-11-04/qualification.json)
retain original run, binary, source and log identities. RedTilde's captured implementation is `2879d4dc` at the
same pin; its before/after fixture and all eight label-background samples match their declared theme/DPI pairs.

| x64 configuration | Eight-suite native run |
| --- | --- |
| Release | `20261104T131240532Z-51192-ce86c6f7618b4f5db53968dfdf9c3ba9` |
| Debug | `20261104T132730103Z-51192-96e06f9ba3874259b8607ec744f534be` |
| ASan Debug | `20261104T134129707Z-51192-98578d8bd4784be9a9c2a636c05dd700` |

The other 11 Release suites pass in `20261104T141751270Z-57212-f4785f3c9df3475da8f755bdc0ae3d03` against
the same compiled profile. All 35 suite executions record zero capability skips. The three ARM64 profiles are
cross-builds, with no local native runtime claim. Unpaired timing/FPS observations do not accept performance.

Durable contracts live in [controls/layout](../../UI/UI_ControlsAndLayout.md),
[performance/resources](../../Core/Core_PerformanceAndResources.md) and
[validation](../../Testing/Testing_Validation.md), with public [rich-text usage](../../../docs/rich-text.md).
Remaining correctness, platform, physical-input and performance acceptance stay in the
[single active plan](../WIP/RemainingWork_2026-11-04.md).
