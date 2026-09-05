# Validation and evidence

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Every supported capability has executable evidence and a truthful status in `capabilities.json`.
`test.ps1` builds the selected configuration, runs Foundation, Embedded and all inherited control suites and records a JSON receipt with
architecture, configuration, time and executable path under `.build/reports`. Failures propagate as nonzero exits.
Every invocation also runs the populated complex-UI benchmark through `performance.ps1` and includes FPS/memory
scenarios and the performance receipt path in each suite receipt. `-PerformanceBaseline` requires a matched
comparison and fails for a suspected regression or invalid evidence; no baseline is explicitly `unpaired`.
Use the [performance contract](../Core/Core_PerformanceAndResources.md) for before/after acceptance and regression advice.
The short WARP benchmark reports completed offscreen throughput, not display refresh or hardware acceptance.
The foundation suite covers timing edge cases, nested stage restoration, reduced motion and injected diagnostics.

`validate-skills.ps1` checks all repository skills with repository-owned code and pinned PyYAML; `validate-specs.ps1`
checks local links, normative documents and plan indexes; `validate-dependencies.ps1` verifies historical origin
metadata, current ownership paths and the exact pending dependency inventory. Original hashes describe the original
commit only: editing owned source must not fail a hash check. New unresolved includes, missing owned files,
developer-local files, escaping/missing build inputs and a second static-library target are rejected. Supported source may not reach
into a consumer. Validators need no personal Codex installation.

The required native matrix is x64/ARM64 and Debug/Release. Cross-compilation is not a runtime pass. CI uses explicit
VS 2026 images with separate native ARM64 execution. Record image/compiler identity in build logs. No fixture changes
real audio/camera defaults, user settings or application data.

As rendering is extracted, add WARP and hardware fixtures for DPI, dirty/clean/hidden behavior, alpha, hostile state,
negative origins, device loss, multi-instance lifetime and failed preparation. Add UIA, IME and real-touch checks.
Preserve imported baselines with their provenance; rebaseline only after reviewing the intended change.
Performance receipts name source/configuration/hardware and include all preparation, texture/cache and recovery costs.

### Inherited coverage and new evidence

`Specs/Done/SourceImport/test-port.json` accounts for all 941 inherited named test cases: 853 reusable runtime cases retained and
88 excluded with individual reasons. Exclusions are application/test-infrastructure utilities not used by DxUi or
source-text assertions; original cases remain available at the recorded source commit. No failed runtime case may
be reclassified merely to obtain a green build. The posted-payload stress case now fills the library's 128-entry
ceiling, with new saturation, wrong-type and stale-token ownership tests in EmbeddedTests.

Tests/Controls retains eight original visual baselines. Native suites cover control state/layout, theme, grids/trees,
animation, native text and IME events, UIA lifetime and menus. Capability skips are emitted in logs and copied into
receipts; a skip is not proof of that capability. The full native matrix executes on x64/ARM64 Debug/Release in CI.
Physical touch, a human IME session and screen-reader interaction are manual adoption checks, not implied by synthetic
messages or a green foundation suite.

EmbeddedTests independently verifies supplied-device rendering and state changes with pixel readback outside the
rendering path, preview/commit/cancel, scaling and resource limits, pool/view isolation and device replacement.
A 1,000-call warmed composition loop intercepts C++ allocation operators and verifies no heap calls, surface
allocations or extra preparations. Its elapsed time is reported, not a machine-independent pass threshold. Readback
and PNG generation are fixture-only operations. The public standalone consumer must compile without private headers.

`Tools/validate_test_port.py` enforces the original case count, unique origins, explicit exclusion reasons and retained/renamed entrypoints. Tooling regression tests verify that deleting a retained case or its disposition fails.

Motion-dependent fixtures explicitly choose an animated theme; reduced-motion fixtures explicitly disable motion.
They never change the user's Windows animation preference. Popup pixel capture waits for the visible final-sized
window so a temporary sizing HWND cannot satisfy visual-baseline readiness. Pixel assertions and original baselines
remain unchanged across runner environments.

Native menu input fixtures wait for a visible popup: the hidden measurement HWND is not ready for input.
Cold creation has a separate five-second setup allowance; owner-message-flood hover and invocation checks
retain their 800 ms deadlines after setup. Capture readiness similarly waits for the final visible surface.

## Independent library workloads

The complex benchmark uses the same synthetic scene as `DxUi.EmbeddedControls.exe --complex-ui`, with no application
services, settings or checkout. EmbeddedTests covers the sample's slider/progress preview and cancellation binding.
Measurements include the scene, benchmark and graphics-helper hashes. Changed fixture identities cannot establish
before/after library regressions. Consumer adoption evidence is owned by its repository; standalone library receipts
live under Measurements and are linked from docs. The external-consumer check renders both sample modes.
