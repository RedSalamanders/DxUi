# Script/color preparation and native filter diagnostics

Status: DONE — SEM-01/RENDER-05 implementation, diagnostic count correction and local functional qualification.
Native implementation: `1352cc7fd244909e1c274b7d567485b9e98d7f1a`, API 5.
Tooling-only follow-up: `010c649b5b73f3d212b7be723f1dc9c78d02141b`.

- [x] Cache target color capability once before staging-raster retirement; retain the documented unsupported
  target fallback without adding a retained interface or allocation to warm drawing.
- [x] Verify actual WARP color pixels and independently observed WIC capability. Exercise controlled
  `E_NOINTERFACE`, absent targets, noncolor scripts, checked failures and cancellation with preserved publication.
- [x] Retain independent fail-before/pass-after evidence using unchanged compiled rendering-probe objects.
- [x] Derive diagnostic counts from the actual twenty-source array and five-scenario execution bound.
- [x] Complete six zero-warning builds, all 66 noninteractive x64 suite executions, actual ASan detection,
  original 680-input/source/native/run member verification and regenerated native gallery provenance.
- [x] Preserve child error output/identity before filter assertions. Verify the owned failing-process regression
  and all eight actual filter cases per x64 configuration against original qualified native binaries and symbols.
- [x] Update rendering, controls, resource, validation and usage contracts; retain rejected disk-full/missing-PDB
  attempts and the twenty-pass/two-failure WIC-assumption cohort without relabelling them.

The [packet](../../../Measurements/ScriptColorPreparation_2026-11-06/README.md) separates native source,
tooling source, original failures and artifact identities. All six gallery images are byte-identical to the
previously reviewed images. Cross-builds do not establish native ARM64 execution. The old hosted ARM64 Grid
failure remains unexplained; its original wrappers/job log are retained and production was withheld.

D6's broader platform policy, native ARM64/physical Windows 10/input/AT, application API-5 migration and formal
paired-performance acceptance remain with the [single active plan](../WIP/RemainingWork_2026-11-04.md).
This closes the existing fallback's implementation and local verification, without asserting those gates.
