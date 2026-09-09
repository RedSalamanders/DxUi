# Shared-library readiness (I19/C1–C2)

## Progress checklist

- [ ] G8: every standalone project and consumer fixture supports Debug/Release/ASan Debug on x64/ARM64.
- [ ] G8: ASAN detects an isolated known defect; native ARM64 runtime qualification recorded.
- [x] G1 implementation and x64 Debug Tooltip/Embedded regressions pass; native deadlines use the current dispatcher clock.
- [ ] G2: required neutral helpers are public; consumer adapters use no private implementation headers.
- [x] G3 native localization implementation and x64 Debug ComboBox/Tree render/geometry tests pass.
- [x] G7 implementation and x64 Debug Accessibility regressions pass; offscreen selected rows retain usable selection providers.
- [x] All 18 x64 Debug suites pass (nine Menu desktop capability skips retained); archive-backed public native-menu helper rerun passes.
- [x] Shared advisory helper and ten deterministic decision/immutable-pin tests pass; consumer wiring remains in I19 C3/C6.
- [ ] Required standalone regression/build matrix, consumer fixture, specs and paired performance qualified.

### G4 clipboard slice

- [x] Accepted policy and current native/embedded paths inspected.
- [x] Retain Debug/Release baseline performance before implementation.
- [x] Reproduce the native 65,536-unit ceiling with a failing regression.
- [x] Separate native transport capacity with checked allocation/Unicode handling.
- [x] Cover native large selections, failures and unchanged embedded ceiling (x64 Debug NativeTextInput passes; broader matrix remains open).
- [x] Update input/testing contracts and hosting documentation; review gallery impact.
- [ ] Run required builds, tests, validators and paired performance comparisons.
- [ ] Record evidence and remaining environment gates; close this library slice when qualified.

Status: ACTIVE. Authorized 2026-09-09. Library implementation owner for I19 C1–C2, starting with G4, in
RedSalamander's `Specs/Plans/WIP/DxUi_SharedLibraryAdoptionAndReleasePlan_2026-09-09.md`.
The cross-repository consumer migration and adoption matrix remain owned by I19;
this library plan does not mark consumer gates complete or update consumer pins.

## Accepted behavior

Native Grid/TextField clipboard transport supports valid selections above 65,536
UTF-16 units, including 100,000 units. Reads stay within the supplied allocation;
validate termination/Unicode, check size arithmetic, and report allocation failures.
Keep one clipboard-open attempt without retry sleeps. Failed cut leaves text intact.
The embedded snapshot/edit ceiling stays 65,536 units and must still reject overflow.

## Implementation and validation

Exercise the production transport through a private test seam, without accessing the
desktop clipboard. Preserve existing test overrides and case identities. Retain
Debug/Release common-fixture baselines before library changes and compare the final
candidate with the same fixture. Add focused native-boundary, 100,000-unit, malformed,
overflow, allocation-failure and contention coverage plus embedded overflow rejection.
Update the owning input/testing contracts and usage docs; no visual change is intended.

Build/test results and limitations will be recorded here as they become available.

Baseline receipts: `.build/reports/G4-baseline-x64-Debug.json`,
`.build/reports/G4-baseline-x64-Release.json`, and
`.build/reports/G4-baseline-x64-Release-repeat.json`. The first Release run was
strongly disturbed (clean rounds 33–1,113 FPS); the repeat before code edits
was stable (2,443–2,551 FPS). Preserve both runs and compare against the repeat.
The regression failed against the old cap after its 65,535/65,536-unit native round trips.
The production transport now uses allocation-bounded capacity and checked byte arithmetic.
Input/testing contracts and hosting usage are updated. Gallery review: no visual or layout
change, so no baseline regeneration is needed. Validation remains in progress.

G8 implementation in progress: all five standalone projects and solution mappings now
include ASan Debug on both architectures, with compiler enforcement in every translation
unit, matching debug CRT, isolated outputs, and an isolated sanitizer detection child.
The first x64 sanitizer build passed and its intentional use-after-free probe was diagnosed correctly.
A concurrent runtime-copy warning was found; staging now has one producer (the archive dependency). This host is x64; native ARM64 execution remains
a required environment gate.

G1 implementation: ported the missing native dispatcher-clock correction and its named idle-host regression
from RedSalamander. Embedded scheduling remains on the existing application clock path. Debug Tooltip/Embedded suites pass.

G7 implementation: aligned offscreen Grid selection getters with pattern availability, using existing
immutable selection IDs without expanding the row cache. Added a 1,000-row case beyond the materialization
budget plus stale-provider rejection after model removal. Debug Accessibility suite passes.

G3 implementation: added owned per-control translations for ComboBox/Tree, retained Grid overrides,
and removed the private numeric-resource fallback cache. Render/geometry regressions cover live
French updates and restoration of default English text without selection/query changes. Existing
English gallery appearance is unchanged; documentation usage is updated. Debug ComboBox/Tree suites pass.

G2 implementation: published the five neutral helper headers with one canonical body, removed
their private-header dependency, added public compilation/link checks, and isolated library helper
symbols from same-named consumer diagnostics/payload/window-message utilities. Consumer adapter
migration and qualification remain open.

Validation update: all 18 x64 Debug suites pass. Menu retains nine unavailable interactive-desktop
cases; these are not runtime qualification. After moving the native-menu host bodies into the archive,
Foundation/public helpers and Menu were rebuilt and rerun successfully. G4 also passes x64 ASan Debug
NativeTextInput with zero capability skips and a verified sanitizer detection probe. The full final
six-configuration matrix and matched performance acceptance remain open.

The shared update helper checks canonical main ancestry and successful push validation, with bounded
read-only requests and at most one advisory. Ten synthetic cases pass, including unavailable/pending/
failed/divergent status and byte-identical pins. It is not wired into a consumer until that product's
unchanged baseline is retained. RedSalamander Full/Fresh Debug baseline capture is currently running.
