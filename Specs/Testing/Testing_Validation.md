# Validation and evidence

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Every supported capability has executable evidence and a truthful status in `capabilities.json`.
`test.ps1` builds the selected configuration, runs the extracted foundation tests and records a JSON receipt with
architecture, configuration, time and executable path under `.build/reports`. Failures propagate as nonzero exits.
The foundation suite covers timing edge cases, nested stage restoration, reduced motion and injected diagnostics.

`validate-skills.ps1` checks all repository skills with repository-owned code and pinned PyYAML; `validate-specs.ps1`
checks local links, normative documents and plan indexes; `validate-dependencies.ps1` verifies import hashes and
forbids active includes reaching into a consumer or the frozen snapshot. They need no personal Codex installation.

The required native matrix is x64/ARM64 and Debug/Release. Cross-compilation is not a runtime pass. CI uses explicit
VS 2026 images with separate native ARM64 execution. Record image/compiler identity in build logs. No fixture changes
real audio/camera defaults, user settings or application data.

As rendering is extracted, add WARP and hardware fixtures for DPI, dirty/clean/hidden behavior, alpha, hostile state,
negative origins, device loss, multi-instance lifetime and failed preparation. Add UIA, IME and real-touch checks.
Preserve imported baselines with their provenance; rebaseline only after reviewing the intended change.
Performance receipts name source/configuration/hardware and include all preparation, texture/cache and recovery costs.
