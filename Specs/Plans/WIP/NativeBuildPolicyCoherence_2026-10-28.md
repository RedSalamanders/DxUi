# Native build policy, terminal results and short DxUi paths

Status: ACTIVE
Date: 2026-10-28
Owner: build maintainers

The user's agreed policy is /Wall with /WX disabled and zero warnings accepted;
Debug uses EditAndContinue, ASan Debug uses ProgramDatabase. Release enables
explicit COMDAT folding (/OPT:ICF); Debug and ASan Debug disable it.
The owning build contract is [the toolchain contract](../../Build/Build_ToolchainAndConsumption.md).
Five repositories implement the same policy without implicitly advancing DxUi pins.

## Checklist

- [x] Preserve the pre-change effective-profile audit and isolate changes from concurrent tooling work.
- [x] Restore RedTilde ARM64 through its normal entrypoint and pass the required policy/lease checks.
- [ ] Enforce and negatively test the common compiler/debug/linker policy in every supported native profile and pinned archive.
- [x] Unify green zero-warning build results, diagnostic/log links and opt-in terminal executable hyperlinks, preserving plain text and existing build/run semantics.
- [x] Shorten source-commit and output-fingerprint folders to 16 hex characters with full identity validation and collision refusal; retain existing data without cleanup.
- [x] Update source discovery, direct helper builds, provenance, test fixtures and dependency identities together.
- [x] Publish zero-warning guidance and normative build/path/terminal contracts, with meaningful tooling tests.
- [ ] Execute affected tests, evaluated profiles, required native builds and applicable runtime qualification; retain every failed result and distinguish cross-builds from runtime tests.
- [ ] Fold/check the changelog; validate formatting/specs/tooling and close this plan only after all required gates pass.

## Decisions and limits

Source pins and full SHA256 identities remain authoritative; folder prefixes are
locators. No source clone is reset for a prefix collision. Existing historical
output is retained in place. Build success is green only after successful
native exit, zero diagnostics and required attestation. Hyperlinks never launch
an application automatically and fall back to quoted launch commands.
The existing explicit RedSalamander Embedded compiler-debug recovery mode remains
an attested exception; ordinary Debug/ASan profiles use the shared defaults.

## Evidence

Baseline audit: E:/DxUi.Perf/flag-coherence-20261028-aea0f188.
Current task evidence: E:/DxUi.Perf/build-policy-evidence-20261028.
RedTilde ARM64 restore and required policy/lease logs are under that task's
RedTilde-ARM64-restore folder; both exited zero before implementation.
The first native /Wall Release run linked successfully but failed the new zero-warning gate.
SDK warning repair includes canonical include paths, explicit enum fallbacks and
conversions, observable test expression order, and declaration-local diagnostic
annotations for existing noncopyable/aggregate traits. Those annotations preserve
public aggregate and overload semantics; static trait checks guard the contract.
No pinned source product is edited. Consumer compatibility remains a separate gate.

Candidate 7c790156 passed all six native builds, all three ordinary Full x64 profiles and Debug/Release focus
profiles. Its ASan focus run passed Menu but lost foreground ownership during NativeTextInput; the foreground
PID was independently identified as Windows ShellExperienceHost. The lease refused the remaining resource
suites and reported failed restoration. Preserve the failure and native images under
`E:/DxUi.Perf/build-policy-evidence-20261028/main-asan-focus4-shell-foreground-failure/`; the cause is not established,
the ASan focus gate remains open, and no assertion or desktop-ownership rule is relaxed for the rerun.

Hosted paired job 123071471655 on de45a598 stopped before measurement because its staged benchmark used a bare
scene include unavailable in the historical baseline's project include profile. The shared header now resolves
the staged scene relative to itself, with a narrowly documented C4464 path-compatibility annotation. A real MSVC
preprocessor control reproduces the original missing include, accepts the staged real headers, preserves the
baseline implementation/project and restores its original harness. This supplements the earlier missing console
module repair. No baseline optimization flag, profile identity, judge, threshold or policy qualification is changed.

Hosted paired job 123103072753 on af669a00 then exposed the second bare dependency: the heap-diagnostic support
header. The preprocessing fixture had supplied a support search directory absent from the baseline. It now
supplies neither private sample nor support search directories, independently reproduces both missing includes,
and accepts both self-locating shared-header paths. The full paired benchmark input list was audited for quoted
includes. All six af669 native builds passed; its runtime parent was stopped while waiting for the desktop mutex,
with only its console host present and no native child running, before integrating this final harness repair.
No runtime pass is claimed for that interrupted batch. Logs, process guards and both no-verdict CI failures remain.

Hosted b4fa57 x64 Debug and ASan consumer qualification failed with C1083 in the relocated sample Main.cpp:
the shared ComplexUiScene.h was copied, but its local directory was absent from the generated external project.
The fixture now adds that copied sample directory while preserving inherited public dependency includes. This
repairs the external fixture's include closure; public headers, library source and the baseline benchmark policy
are unchanged. Actual relocated Debug/Release/ASan builds and rendering/conformance checks remain required.
