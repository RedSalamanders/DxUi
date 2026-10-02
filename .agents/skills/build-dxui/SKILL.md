---
name: build-dxui
description: Build, test or diagnose the standalone DxUi library and consumer outputs; use for toolchain/configuration failures.
---

# Build dxui

Read [the owning contract](../../../Specs/Build/Build_ToolchainAndConsumption.md) and [AGENTS.md](../../../AGENTS.md).

Use root build.ps1 and test.ps1. Select x64/ARM64 and Debug/Release/ASan Debug explicitly. Discover Visual Studio with prerelease support. Read the log for the first compiler error. Build the single DxUi.lib and run the Foundation, controls and embedded suites; Foundation alone does not validate control or rendering changes. Cross-build receipts must not imply native execution.
Restore with `vcpkg-install.ps1` first. It builds the dependencies with the Visual Studio installation and default MSVC toolset that build.ps1's MSBuild uses, not the newest toolset vcpkg finds, through an overlay triplet it writes under the output root (`vcpkg-triplets/<platform>/<triplet>.cmake`; the first restore after a change rebuilds the packages). It prints the installation and toolset it pinned; a restore that fails starts at the first vcpkg configure log under `vcpkg_buildtrees`, and a missing or malformed `VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt` fails before vcpkg runs. Never hard-code an installation or toolset, or set `VCPKG_OVERLAY_TRIPLETS`, to get around a restore.
Iterate on one control test with `test.ps1 -Suites <Suite> -Tests <Name>` (`DxUi.ControlTests.exe --suite=<Suite> --test=<Name>`);
an unknown name fails the run, and a filtered run never replaces a suite's receipt. Register every new control test as
`DXUI_RUN_TEST(TestName);` in its suite runner so the filter can select it and the watchdog can bound it: a test that
outlives its deadline (300 s; `--test-timeout=<seconds>`, `test.ps1 -TestTimeout`, 0 off) ends the run with
`TIMEOUT: <TestName> after <N> s` and exit code 124. A driver thread of a blocking menu starts with `DismissMenusIfDriverFails`.
The suites that need the person's real desktop (`Menu`, `NativeTextInput` and the two menu resource fixtures) run deliberately through
`test.ps1 -Interactive` (`-Suites` to pick, `-SkipBuild` after a build): it refuses without an interactive desktop, asks first (Cancel
is the default), warns while it runs and restores the foreground window, focus and pointer however the run ends. Run it, or any of those
suites, only when the person at the desktop has agreed to the time; leave them out of `-Suites` otherwise.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not replace consumer product qualification or deferred native-platform/IME/AT coverage.
