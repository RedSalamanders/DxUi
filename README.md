# DxUi

Shared native Windows DirectX controls for RedXe and RedSalamander.

Repository: **private**, `RedSalamanders/DxUI`, default branch **main**. The preferred local checkout is `Z:\src\DxUI`, beside the two
application checkouts. Builds also work from other paths.

RedXe is the first planned application consumer, through AV Control. RedSalamander will migrate later.
The integration decision is pinned source compiled into static libraries; no shared DxUi runtime DLL is required.

## Current state

The repository is bootstrapped with agent guidance, focused skills, normative design contracts, active plans,
validators, CI and an independently built `DxUi.Foundation.lib`. That first extracted target contains the existing
frame clock, frame-stage and reduced-motion behavior with application-independent diagnostics.

The original control and test source is preserved under `upstream/RedSalamander` with commit and SHA-256 provenance.
It is a frozen extraction reference and is **not** a supported standalone control library yet. Full control
decoupling, embedded D3D11 hosting, RedXe input/text/UIA bridges and AV integration remain in the active plan.
Neither application has been ported by this bootstrap. Do not advertise browser mockup behavior as native support.

## Build and validate

Requires Windows, Visual Studio 2026 with v145 C++ tools and Windows SDK 10.0.26100.0. Use x64 or ARM64.
Validation requires Python 3.11+ and the pinned PyYAML dependency:

```powershell
py -3 -m pip install -r Tools/requirements-validation.txt
.\validate-skills.ps1
.\validate-specs.ps1
.\validate-dependencies.ps1
.\test.ps1 -Configuration Debug -Platform x64
.\test.ps1 -Configuration Release -Platform x64
.\build.ps1 -Configuration Debug -Platform ARM64
.\build.ps1 -Configuration Release -Platform ARM64
```

`test.ps1` refuses to call a cross-build an executed ARM64 test. Run native ARM64 tests on an ARM64 machine or the
configured GitHub ARM64 job. Outputs, scratch and reports stay under `.build/`. No tool needs a personal Codex path.

Start with [AGENTS.md](AGENTS.md), [spec authority](Specs/README.md), and the
[active plan](Specs/Plans/WIP/BootstrapAndRedXeAdoption_2026-09-05.md). For consumer setup see
[the build contract](Specs/Build/Build_ToolchainAndConsumption.md).
