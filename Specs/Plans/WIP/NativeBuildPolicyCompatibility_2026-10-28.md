# Native build policy compatibility - 2026-10-28

Status: active. API revision 4 is preserved for this existing consumer pin.
Owning contract: [Win32 host](../../Rendering/Rendering_Win32Host.md).

- [x] Canonicalize owned private include paths without importing newer SDK features.
- [x] Preserve existing aggregate/copy traits with declaration-local diagnostic annotations.
- [x] Make HRESULT ternary boundaries explicit without changing result bits.
- [x] Replace folded-function type identity with mutable data tags.
- [x] Add the WindowHost Release folding and pinned-lifetime regression.
- [x] Retain controlled before/no-ICF, before/ICF failure, and after/ICF pass evidence.
- [ ] Complete applicable native configuration and functional validation.
- [ ] Update the consuming project's exact pin through normal restore and validate that application.

Evidence is retained under E:/DxUi.Perf/build-policy-evidence-20261028. The isolated controlled reproducer
fails the original ICF build with exit 8, passes the non-ICF control, and passes the data-tag candidate while
confirming folded=1. The permanent WindowHost Release regression also passes with folded=1.
Historical SDK APIs and their unrelated capability gates stay at this pin; this branch does not adopt
newer controls or change API 3 into API 4. Cross-builds do not establish native ARM64 runtime support.

The first Release focus attempt exposed older popup-readiness and IMM-setup fixtures. Backport only test/support/tooling changes from upstream 3893daa6 and ee63697c; keep src/include byte-identical to the ICF maintenance candidate. Preserve all predicates and existing timing bounds. Required fresh matrix, ordinary suites, zero-skip focus profiles and tooling qualification remain pending for this followup.
