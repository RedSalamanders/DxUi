# Getting started

Install Visual Studio 2026 with the Desktop development with C++ workload, v145, Windows SDK 10.0.26100,
and the Clang tools for formatting. Use Windows 10/11, Unicode, `stdcpplatest`, and a matching x64 or ARM64 target.
Git restores the repository-pinned vcpkg/WIL dependency. Python 3.11+ and
[validation requirements](../Tools/requirements-validation.txt) are needed for repository validators.

```powershell
.\vcpkg-install.ps1 -Platform x64
.\build.ps1 -Configuration Debug -Platform x64
.\.build\x64\Debug\DxUi.EmbeddedControls.exe
.\test.ps1 -Configuration Debug -Platform x64
.\gallery.ps1 -SkipBuild
```

The example opens a toggle and a slider. `--output image.png` renders it headlessly using WARP.
Artifacts live below `.build`; the library is `.build/<Platform>/<Configuration>/DxUi.lib`.
Use `-Platform ARM64` to cross-build on x64; execute those binaries on a native ARM64 Windows host.

## Consume the library

Pin a tested full Git commit, API revision **2** and target `["DxUi"]` in the consumer's
`Dependencies/DxUi.lock.json`. Follow the exact schema and fingerprint validation in
[the consumption contract](../Specs/Build/Build_ToolchainAndConsumption.md) and
[consumer validator](../Tools/validate_consumer.ps1); branch names are not release pins.

From the consumer directory, create the minimal implemented lock against a clean, validated library checkout:

```powershell
$libraryRoot = (Resolve-Path ..\DxUI).Path
$revision = (git -C $libraryRoot rev-parse HEAD).Trim()
New-Item -ItemType Directory -Path Dependencies -Force | Out-Null
@{
    repository = 'https://github.com/RedSalamanders/DxUI'
    commit = $revision
    apiRevision = 2
    targets = @('DxUi')
} | ConvertTo-Json | Set-Content Dependencies/DxUi.lock.json -Encoding utf8
```

The validator checks repository identity, exact commit, clean source, API revision and target. The consumer must
also preserve the toolchain/dependency fingerprint in its build evidence/output root as required by the contract.

Set `DxUiRoot`, `DxUiConsumerLockFile`, and an isolated `DxUiConsumerOutputRoot` before importing
[DxUi.Consumer.props](../Build/DxUi.Consumer.props) after `Microsoft.Cpp.props`, then
[DxUi.Consumer.targets](../Build/DxUi.Consumer.targets) after `Microsoft.Cpp.targets`.
Reference [DxUi.vcxproj](../src/DxUi.vcxproj) once. Restore with `vcpkg-install.ps1 -OutputRoot` pointing at that same
isolated root. Use matching architecture, toolset, configuration and `/MDd` (Debug) or `/MD` (Release).
Do not enumerate library `.cpp` files or copy its source into your application.

## Create a control tree

```cpp
#include <DxUi/Embedded.h>

// After successful view.Attach(graphics), on the UI thread:
auto root = std::make_unique<DxUi::Panel>();
auto* title = root->AddChild<DxUi::Label>(L"Settings");
title->SetBounds(D2D1::RectF(16, 12, 304, 44));
auto* toggle = root->AddChild<DxUi::Toggle>(L"Enabled");
toggle->SetBounds(D2D1::RectF(16, 52, 304, 92));
toggle->SetChecked(true);
toggle->SetOnToggled([](bool checked) { /* update your application model */ });
view.Controls().SetRoot(std::move(root));
```

The root owns descendants. `AddChild` returns a borrowed pointer, valid only while that child remains in the tree.
Bounds use host-space device-independent pixels (DIPs), including children of ordinary Panels; a Panel does not
automatically offset child bounds by its own origin. Host preparation uses physical pixels and an explicit DPI.
Keep callback captures and borrowed models alive until detached or removed. Use the
[hosting guide](hosting.md) to attach graphics, prepare, composite and route input; constructing a tree alone
does not schedule rendering or create a window.

For generic tooling, include [ControlCatalog.h](../include/DxUi/ControlCatalog.h), enumerate `GetControlCatalog()`
and call `CreateControl(kind, result)`. Check the HRESULT. Factories create empty controls; populate their content
and models before displaying them.
